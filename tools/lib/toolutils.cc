/*
 * toolutils.{cc,hh} -- utility routines for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "straccum.hh"
#include "routert.hh"
#include "lexert.hh"
#include "toolutils.hh"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

String
file_string(FILE *f, ErrorHandler *errh)
{
  StringAccum sa;
  while (!feof(f))
    if (char *x = sa.reserve(4096)) {
      size_t r = fread(x, 1, 4096, f);
      sa.forward(r);
    } else {
      if (errh)
	errh->error("file too large, out of memory");
      return String();
    }
  return sa.take_string();
} 

String
file_string(const char *filename, ErrorHandler *errh)
{
  FILE *f;
  if (filename && strcmp(filename, "-") != 0) {
    f = fopen(filename, "r");
    if (!f) {
      if (errh)
	errh->error("%s: %s", filename, strerror(errno));
      return String();
    }
  } else {
    f = stdin;
    filename = "<stdin>";
  }

  String s;
  if (errh) {
    PrefixErrorHandler perrh(errh, filename + String(": "));
    s = file_string(f, &perrh);
  } else
    s = file_string(f);
  
  if (f != stdin)
    fclose(f);
  return s;
}

RouterT *
read_router_file(const char *filename, ErrorHandler *errh, RouterT *router)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();

  // read file string
  int old_nerrors = errh->nerrors();
  String s = file_string(filename, errh);
  if (!s && errh->nerrors() != old_nerrors)
    return 0;

  // check for archive
  Vector<ArchiveElement> archive;
  if (s.length() && s[0] == '!') {
    separate_ar_string(s, archive, errh);
    int found = -1;
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name == "config")
	found = i;
    if (found >= 0)
      s = archive[found].data;
    else {
      errh->error("archive has no `config' section");
      s = String();
    }
  }

  // read router
  LexerT lexer(errh);
  lexer.reset(s);
  if (router)
    lexer.set_router(router);
  while (lexer.ystatement()) ;
  router = lexer.take_router();

  // add archive bits
  if (router && archive.size()) {
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name != "config")
	router->add_archive(archive[i]);
  }

  // done
  return router;
}

void
write_router_file(RouterT *r, FILE *f, ErrorHandler *errh)
{
  if (!r) return;
  
  String config_str = r->configuration_string();
  
  // create archive if necessary
  const Vector<ArchiveElement> &archive = r->archive();
  if (archive.size()) {
    Vector<ArchiveElement> narchive;
    
    // add configuration
    ArchiveElement config_ae;
    config_ae.name = "config";
    config_ae.date = time(0);
    config_ae.uid = geteuid();
    config_ae.gid = getegid();
    config_ae.mode = 0644;
    config_ae.data = config_str;
    narchive.push_back(config_ae);
    
    // add other archive elements
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name && archive[i].name != "config")
	narchive.push_back(archive[i]);
    
    config_str = create_ar_string(narchive, errh);
  }
  
  fwrite(config_str.data(), 1, config_str.length(), f);
}

int
write_router_file(RouterT *r, const char *name, ErrorHandler *errh)
{
  if (name && strcmp(name, "-") != 0) {
    FILE *f = fopen(name, "w");
    if (!f) {
      if (errh)
	errh->error("%s: %s", name, strerror(errno));
      return -1;
    }
    write_router_file(r, f, errh);
    fclose(f);
  } else
    write_router_file(r, stdout, errh);
  return 0;
}

String
unique_tmpnam(const String &pattern, ErrorHandler *errh)
{
  String tmpdir;
  if (const char *path = getenv("TMPDIR"))
    tmpdir = path;
#ifdef P_tmpdir
  else if (P_tmpdir)
    tmpdir = P_tmpdir;
#endif
  else
    tmpdir = "/tmp";

  int star_pos = pattern.find_left('*');
  String left, right;
  if (star_pos >= 0) {
    left = "/" + pattern.substring(0, star_pos);
    right = pattern.substring(star_pos + 1);
  } else
    left = "/" + pattern;
  
  int uniqueifier = getpid();
  while (1) {
    String name = tmpdir + left + String(uniqueifier) + right;
    int result = open(name.cc(), O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
    if (result >= 0) {
      close(result);
      remove_file_on_exit(name);
      return name;
    } else if (errno != EEXIST) {
      errh->error("cannot create temporary file: %s", strerror(errno));
      return String();
    }
    uniqueifier++;
  }
}

static Vector<String> *remove_files;

static void
remover(String fn)
{
  struct stat s;
  if (stat(fn.cc(), &s) < 0)
    return;
  if (S_ISDIR(s.st_mode)) {
    DIR *dir = opendir(fn.cc());
    if (!dir)
      return;
    while (struct dirent *d = readdir(dir)) {
      if (d->d_name[0] == '.'
	  && (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
	continue;
      remover(fn + "/" + d->d_name);
    }
    closedir(dir);
    rmdir(fn.cc());
  } else
    unlink(fn.cc());
}

extern "C" {
static void
signal_handler(int)
{
  exit(2);
}

static void
atexit_remover(void)
{
  if (remove_files) {
    for (int i = 0; i < remove_files->size(); i++)
      remover((*remove_files)[i]);
  }
}
}

void
remove_file_on_exit(const String &file)
{
  if (file) {
    if (!remove_files) {
      remove_files = new Vector<String>;
      signal(SIGINT, signal_handler);
      signal(SIGTERM, signal_handler);
      signal(SIGPIPE, signal_handler);
      atexit(atexit_remover);
    }
    remove_files->push_back(file);
  }
}

static String
path_find_file_2(const String &filename, String path, String &default_path,
		 String subdir)
{
  if (subdir && subdir.back() != '/') subdir += "/";
  
  while (1) {
    int colon = path.find_left(':');
    String dir = (colon < 0 ? path : path.substring(0, colon));
    
    if (!dir && default_path) {
      // look in default path
      String was_default_path = default_path;
      default_path = String();
      String s = path_find_file_2(filename, was_default_path, default_path, 0);
      if (s) return s;
      
    } else if (dir) {
      if (dir.back() != '/') dir += "/";
      // look for `dir/subdir/filename' and `dir/subdir/click/filename'
      if (subdir) {
	String name = dir + subdir + filename;
	if (access(name.cc(), F_OK) >= 0)
	  return name;
	name = dir + subdir + "click/" + filename;
	if (access(name.cc(), F_OK) >= 0)
	  return name;
      }
      // look for `dir/filename'
      String name = dir + filename;
      if (access(name.cc(), F_OK) >= 0)
	return name;
    }
    
    if (colon < 0) return String();
    path = path.substring(colon + 1);
  }
}

String
clickpath_find_file(const String &filename, const char *subdir,
		    String default_path, ErrorHandler *errh = 0)
{
  const char *path = getenv("CLICKPATH");
  String was_default_path = default_path;
  String s;
  if (path)
    s = path_find_file_2(filename, path, default_path, subdir);
  if (!s && !path && default_path) {
    default_path = String();
    s = path_find_file_2(filename, was_default_path, default_path, 0);
  }
  if (!s && subdir
      && (strcmp(subdir, "bin") == 0 || strcmp(subdir, "sbin") == 0))
    if (const char *path_variable = getenv("PATH"))
      s = path_find_file_2(filename, path_variable, default_path, 0);
  if (!s && errh) {
    // three error messages for three different situations:
    if (default_path) {
      // CLICKPATH set, left no opportunity to use default path
      errh->message("cannot find file `%s'", String(filename).cc());
      errh->fatal("in CLICKPATH `%s'", path);
    } else if (!path) {
      // CLICKPATH not set
      errh->message("cannot find file `%s'", String(filename).cc());
      errh->fatal("in installed location `%s'", was_default_path.cc());
      errh->fatal("(try setting the CLICKPATH environment variable)");
    } else {
      // CLICKPATH set, left opportunity to use default pathb
      errh->message("cannot find file `%s'", String(filename).cc());
      errh->fatal("in CLICKPATH or `%s'", was_default_path.cc());
    }
  }
  return s;
}

String
click_mktmpdir(ErrorHandler *errh = 0)
{
  String tmpdir;
  if (const char *path = getenv("TMPDIR"))
    tmpdir = path;
#ifdef P_tmpdir
  else if (P_tmpdir)
    tmpdir = P_tmpdir;
#endif
  else
    tmpdir = "/tmp";
  
  int uniqueifier = getpid();
  while (1) {
    String tmpsubdir = tmpdir + "/clicktmp" + String(uniqueifier);
    int result = mkdir(tmpsubdir.cc(), 0700);
    if (result >= 0) {
      remove_file_on_exit(tmpsubdir);
      return tmpsubdir;
    }
    if (result < 0 && errno != EEXIST) {
      if (errh)
	errh->fatal("cannot create temporary directory: %s", strerror(errno));
      return String();
    }
    uniqueifier++;
  }
}

ArchiveElement
init_archive_element(const String &name, int mode)
{
  ArchiveElement ae;
  ae.name = name;
  ae.date = time(0);
  ae.uid = geteuid();
  ae.gid = getegid();
  ae.mode = mode;
  ae.data = String();
  return ae;
}

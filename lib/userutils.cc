/*
 * userutils.{cc,hh} -- utility routines for user-level + tools
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
#include "confparse.hh"
#include "userutils.hh"
#include "error.hh"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdarg.h>

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
    f = fopen(filename, "rb");
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

void
parse_tabbed_lines(const String &str, Vector<String> *fields1, ...)
{
  va_list val;
  Vector<void *> tabs;
  va_start(val, fields1);
  while (fields1) {
    tabs.push_back((void *)fields1);
    fields1 = va_arg(val, Vector<String> *);
  }
  va_end(val);
  
  int p = 0;
  int len = str.length();
  
  while (p < len) {
    // read a line
    int endp = str.find_left('\n', p);
    if (endp < 0)
      endp = str.length();
    String line = str.substring(p, endp - p);

    // break into words
    Vector<String> words;
    cp_spacevec(line, words);

    // skip blank lines & comments
    if (words.size() > 0 && words[0][0] != '#')
      for (int i = 0; i < tabs.size(); i++) {
	Vector<String> *vec = (Vector<String> *)tabs[i];
	if (i < words.size())
	  vec->push_back(cp_unquote(words[i]));
	else
	  vec->push_back(String());
      }

    // skip past end of line
    p = endp + 1;
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

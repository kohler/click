// -*- c-basic-offset: 4; related-file-name: "../include/click/userutils.hh"-*-
/*
 * userutils.{cc,hh} -- utility routines for user-level + tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/pathvars.h>

#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/userutils.hh>
#include <click/error.hh>
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
#include <stdlib.h>

#if HAVE_DYNAMIC_LINKING && defined(HAVE_DLFCN_H)
# include <dlfcn.h>
#endif

CLICK_DECLS


bool
glob_match(const String &str, const String &pattern)
{
    const char *sdata = str.data();
    const char *pdata = pattern.data();
    int slen = str.length();
    int plen = pattern.length();
    int spos = 0, ppos = 0;
    Vector<int> glob_ppos, glob_spos1, glob_spos2;

    while (1) {
	while (ppos < plen)
	    switch (pdata[ppos]) {

	      case '?':
		if (spos >= slen)
		    goto done;
		spos++;
		ppos++;
		break;

	      case '*':
		glob_ppos.push_back(ppos + 1);
		glob_spos1.push_back(spos);
		glob_spos2.push_back(slen);
		spos = slen;
		ppos++;
		break;

	      case '[': {
		  if (spos >= slen)
		      goto done;

		  // find end of character class
		  int p = ppos + 1;
		  bool negated = false;
		  if (p < plen && pdata[p] == '^') {
		      negated = true;
		      p++;
		  }
		  int first = p;
		  if (p < plen && pdata[p] == ']')
		      p++;
		  while (p < plen && pdata[p] != ']')
		      p++;
		  if (p >= plen) // not a character class at all
		      goto ordinary;
	 
		  // parse character class
		  bool in = false;
		  for (int i = first; i < p && !in; i++) {
		      int c1 = pdata[i];
		      int c2 = c1;
		      if (i < p - 2 && pdata[i+1] == '-') {
			  c2 = pdata[i+2];
			  i += 2;
		      }
		      if (sdata[spos] >= c1 && sdata[spos] <= c2)
			  in = true;
		  }

		  if ((negated && in) || (!negated && !in))
		      goto done;
		  ppos = p + 1;
		  spos++;
		  break;
	      }

	      default:
	      ordinary:
		if (spos >= slen || sdata[spos] != pdata[ppos])
		    goto done;
		spos++;
		ppos++;
		break;
	
	    }

      done:
	if (spos == slen && ppos == plen)
	    return true;
	while (glob_ppos.size() && glob_spos1.back() == glob_spos2.back()) {
	    glob_ppos.pop_back();
	    glob_spos1.pop_back();
	    glob_spos2.pop_back();
	}
	if (glob_ppos.size()) {
	    glob_spos2.back()--;
	    spos = glob_spos2.back();
	    ppos = glob_ppos.back();
	} else
	    return false;
    }
}

String
percent_substitute(const String &pattern, int format1, ...)
{
  const char *results[256];
  memset(&results, 0, sizeof(results));

  va_list val;
  va_start(val, format1);
  while (format1 > 0) {
    assert(!results[format1]);
    results[format1] = va_arg(val, const char *);
    format1 = va_arg(val, int);
  }
  va_end(val);

  results[(uint8_t)'%'] = "%";

  const char *s = pattern.begin();
  const char *end = pattern.end();
  StringAccum sa;
  while (s < end) {
    const char *pct = find(s, end, '%');
    if (pct >= end - 1)
      break;
    int format = (unsigned char)(pct[1]);
    if (const char *str = results[format])
      sa << pattern.substring(s, pct) << str;
    else
      sa << pattern.substring(s, pct + 2);
    s = pct + 2;
  }

  if (s == pattern.begin())
    return pattern;
  else {
    sa << pattern.substring(s, pattern.end());
    return sa.take_string();
  }
}

int
click_strcmp(const String &a, const String &b)
{
    const char *ad = a.data(), *ae = a.data() + a.length();
    const char *bd = b.data(), *be = b.data() + b.length();
    
    while (ad < ae && bd < be) {
	if (isdigit(*ad) && isdigit(*bd)) {
	    // compare the two numbers, but don't treat them as numbers in
	    // case of overflow
	    // first, skip initial '0's
	    const char *iad = ad, *ibd = bd;
	    while (ad < ae && *ad == '0')
		ad++;
	    while (bd < be && *bd == '0')
		bd++;
	    int longer_zeros = (ad - iad) - (bd - ibd);
	    // skip to end of number
	    const char *nad = ad, *nbd = bd;
	    while (ad < ae && isdigit(*ad))
		ad++;
	    while (bd < be && isdigit(*bd))
		bd++;
	    // longer number must be larger
	    if ((ad - nad) != (bd - nbd))
		return (ad - nad) - (bd - nbd);
	    // otherwise, compare numbers with the same length
	    for (; nad < ad && nbd < bd; nad++, nbd++)
		if (*nad != *nbd)
		    return *nad - *nbd;
	    // finally, longer string of initial '0's wins
	    if (longer_zeros != 0)
		return longer_zeros;
	} else if (isdigit(*ad))
	    return (isalpha(*bd) ? -1 : 1);
	else if (isdigit(*bd))
	    return (isalpha(*ad) ? 1 : -1);
	else {
	    int d = tolower(*ad) - tolower(*bd);
	    if (d != 0)
		return d;
	    ad++;
	    bd++;
	}
    }

    if ((ae - ad) != (be - bd))
	return (ae - ad) - (be - bd);
    else {
	assert(a.length() == b.length());
	return memcmp(a.data(), b.data(), a.length());
    }
}

const char *
filename_landmark(const char *filename, bool file_is_expr)
{
    if (file_is_expr)
	return "<expr>";
    else if (!filename || !*filename || strcmp(filename, "-") == 0)
	return "<stdin>";
    else
	return filename;
}

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
file_string(String filename, ErrorHandler *errh)
{
  FILE *f;
  if (filename && filename != "-") {
    f = fopen(filename.c_str(), "rb");
    if (!f) {
      if (errh)
	errh->error("%s: %s", filename.c_str(), strerror(errno));
      return String();
    }
  } else {
    f = stdin;
    filename = "<stdin>";
  }

  String s;
  if (errh) {
    PrefixErrorHandler perrh(errh, filename + ": ");
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

  const char *star = find(pattern, '*');
  String left, right;
  if (star < pattern.end()) {
    left = "/" + pattern.substring(pattern.begin(), star);
    right = pattern.substring(star + 1, pattern.end());
  } else
    left = "/" + pattern;
  
  int uniqueifier = getpid();
  while (1) {
    String name = tmpdir + left + String(uniqueifier) + right;
    int result = open(name.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
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

// 19.Aug.2003 - atexit() is called after String::static_cleanup(), so use
// char *s instead of Strings
static Vector<char *> *remove_files;

static void
remover(char *fn)
{
    struct stat s;
    if (stat(fn, &s) < 0)
	return;
    if (S_ISDIR(s.st_mode)) {
	DIR *dir = opendir(fn);
	if (!dir)
	    return;
	while (struct dirent *d = readdir(dir)) {
	    if (d->d_name[0] == '.'
		&& (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
		continue;
	    if (char *nfn = new char[strlen(fn) + strlen(d->d_name) + 2]) {
		sprintf(nfn, "%s/%s", fn, d->d_name);
		remover(nfn);
		delete[] nfn;
	    }
	}
	closedir(dir);
	rmdir(fn);
    } else
	unlink(fn);
}

extern "C" {
static void
signal_handler(int)
{
  exit(2);
}

static void
atexit_remover()
{
    if (remove_files)
	for (int i = 0; i < remove_files->size(); i++) {
	    remover((*remove_files)[i]);
	    delete[] (*remove_files)[i];
	}
}
}

void
remove_file_on_exit(const String &file)
{
    if (file) {
	if (!remove_files) {
	    remove_files = new Vector<char *>;
	    signal(SIGINT, signal_handler);
	    signal(SIGTERM, signal_handler);
	    signal(SIGPIPE, signal_handler);
	    atexit(atexit_remover);
	}
	if (char *x = new char[file.length() + 1]) {
	    memcpy(x, file.data(), file.length());
	    x[file.length()] = 0;
	    remove_files->push_back(x);
	}
    }
}


static const char* the_clickpath = 0;

const char *
clickpath()
{
    if (!the_clickpath) {
	the_clickpath = getenv("CLICKPATH");
	if (!the_clickpath)
	    the_clickpath = "";
    }
    return the_clickpath;
}

void
set_clickpath(const char* p)
{
    // small memory leak, since we cannot be sure that putenv() copies its
    // argument
    char* env = new char[strlen(p) + 11];
    sprintf(env, "CLICKPATH=%s", p);
    putenv(env);
    the_clickpath = env + 10;
}


static bool
path_find_file_2(const String &filename, const String &path,
		 String default_path, String subdir,
		 Vector<String>& results, bool exit_early)
{
    if (subdir && subdir.back() != '/')
	subdir += "/";

    const char *begin = path.begin();
    const char *end = path.end();
    int before_size = results.size();
    
    do {
	String dir = path.substring(begin, find(begin, end, ':'));
	begin = dir.end() + 1;
    
	if (!dir && default_path) {
	    // look in default path
	    if (path_find_file_2(filename, default_path, "", 0, results, exit_early) && exit_early)
		return true;
      
	} else if (dir) {
	    if (dir.back() != '/')
		dir += "/";
	    String fn;
	    // look for 'dir/subdir/click/filename' and 'dir/subdir/filename'
	    if (subdir) {
		fn = dir + subdir + "click/" + filename;
		if (access(fn.c_str(), F_OK) >= 0)
		    goto found;
		fn = dir + subdir + filename;
		if (access(fn.c_str(), F_OK) >= 0)
		    goto found;
	    }
	    // look for 'dir/filename'
	    fn = dir + filename;
	    if (access(fn.c_str(), F_OK) >= 0) {
	      found:
		results.push_back(fn);
		if (exit_early)
		    return true;
	    }
	}
    } while (begin < end);

    return results.size() == before_size;
}


String
clickpath_find_file(const String &filename, const char *subdir,
		    String default_path, ErrorHandler *errh)
{
    const char *path = clickpath();
    String was_default_path = default_path;

    if (filename && filename[0] == '/')
	return filename;
    if (!path && default_path)
	path = ":";
    Vector<String> fns;
    path_find_file_2(filename, path, default_path, subdir, fns, true);

    // look in 'PATH' for binaries
    if (!fns.size() && subdir
	&& (strcmp(subdir, "bin") == 0 || strcmp(subdir, "sbin") == 0))
	if (const char *path_variable = getenv("PATH"))
	    path_find_file_2(filename, path_variable, "", 0, fns, true);
  
    if (!fns.size() && errh) {
	if (default_path) {
	    // CLICKPATH set, left no opportunity to use default path
	    errh->fatal("cannot find file '%s'\nin CLICKPATH '%s'", filename.c_str(), path);
	} else if (!path) {
	    // CLICKPATH not set
	    errh->fatal("cannot find file '%s'\nin install directory '%s'\n(Try setting the CLICKPATH environment variable.)", filename.c_str(), was_default_path.c_str());
	} else {
	    // CLICKPATH set, left opportunity to use default pathb
	    errh->fatal("cannot find file '%s'\nin CLICKPATH or '%s'", filename.c_str(), was_default_path.c_str());
	}
    }
    
    return fns.size() ? fns[0] : String();
}

void
clickpath_expand_path(const char* subdir, const String& default_path, Vector<String>& result)
{
    const char* path = clickpath();
    if (!path && default_path)
	path = ":";
    int first = result.size();
    path_find_file_2(".", path, default_path, subdir, result, false);
    // remove trailing '/.'s
    for (String* x = result.begin() + first; x < result.end(); x++)
	*x = x->substring(x->begin(), x->end() - 2);
}

bool
path_allows_default_path(String path)
{
    const char *begin = path.begin();
    const char *end = path.end();
    while (1) {
	const char *colon = find(begin, end, ':');
	if (colon == begin)
	    return true;
	else if (colon == end)
	    return false;
	else
	    begin = colon + 1;
    }
}

String
click_mktmpdir(ErrorHandler *errh)
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
	int result = mkdir(tmpsubdir.c_str(), 0700);
	if (result >= 0) {
	    remove_file_on_exit(tmpsubdir);
	    return tmpsubdir + "/";
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

    const char *begin = str.begin();
    const char *end = str.end();
  
    while (begin < end) {
	// read a line
	String line = str.substring(begin, find(begin, end, '\n'));
	begin = line.end() + 1;

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


bool
compressed_data(const unsigned char *buf, int len)
{
    return (len >= 3
	    && ((buf[0] == 037 && buf[1] == 0235)
		|| (buf[0] == 037 && buf[1] == 0213)
		|| (buf[0] == 'B' && buf[1] == 'Z' && buf[2] == 'h')));
}

FILE *
open_uncompress_pipe(const String &filename, const unsigned char *buf, int, ErrorHandler *errh)
{
    String command;
    if (buf[0] == 'B')
	command = "bzcat " + filename;
    else if (access("/usr/bin/gzcat", X_OK) >= 0)
	command = "/usr/bin/gzcat " + filename;
    else 
	command = "zcat " + filename;
    if (FILE *p = popen(command.c_str(), "r"))
	return p;
    else {
	errh->error("'%s': %s", command.c_str(), strerror(errno));
	return 0;
    }
}

enum {
    COMP_GZIP = 1,
    COMP_BZ2 = 2,
};
int
check_suffix_compressed(const String &filename)
{
    String suffix3 = filename.substring(filename.length() - 3, 3);
    String suffix4 = filename.substring(filename.length() - 4, 4);
    if (suffix3 == ".gz") {
	return COMP_GZIP;
    } else if (suffix4 == ".bz2") {
	return COMP_BZ2;
    } else {
	return 0;
    }
}
FILE *
open_compress_pipe(const String &filename, ErrorHandler *errh)
{
    String command;
    int c = check_suffix_compressed(filename);
    if (!c) {
	return 0;
    }
    if (c == COMP_GZIP) {
	command = "/bin/gzip > " + filename;
    } else if (c == COMP_BZ2) {
	command = "/usr/bin/bzip2 > " + filename;
    } else {
	errh->error("unknown file extension: %s (%d)", filename.c_str(), c);
	return 0;
    }
    if (FILE *p = popen(command.c_str(), "w"))
	return p;
    else {
	errh->error("'%s': %s", command.c_str(), strerror(errno));
	return 0;
    }
}

#if HAVE_DYNAMIC_LINKING

extern "C" {
typedef int (*init_module_func)(void);
}

int
clickdl_load_package(String package, ErrorHandler *errh)
{
#ifndef RTLD_GLOBAL
# define RTLD_GLOBAL 0
#endif
#ifndef RTLD_NOW
  void *handle = dlopen((char *)package.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#else
  void *handle = dlopen((char *)package.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
  if (!handle)
    return errh->error("package %s", dlerror());
  void *init_sym = dlsym(handle, "init_module");
  if (!init_sym)
    return errh->error("package '%s' has no 'init_module'", package.c_str());
  init_module_func init_func = (init_module_func)init_sym;
  if ((*init_func)() != 0)
    return errh->error("error initializing package '%s'", package.c_str());
  return 0;
}

#endif

CLICK_ENDDECLS

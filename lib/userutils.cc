// -*- c-basic-offset: 4; related-file-name: "../include/click/userutils.hh"-*-
/*
 * userutils.{cc,hh} -- utility routines for user-level + tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Meraki, Inc.
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
    const char *send = str.end();
    const char *pend = pattern.end();

    // quick common-case check for suffix matches
    while (pattern.begin() < pend && str.begin() < send
	   && pend[-1] != '*' && pend[-1] != '?' && pend[-1] != ']'
	   && (pattern.begin() + 1 == pend || pend[-2] != '\\'))
	if (pend[-1] == send[-1])
	    --pend, --send;
	else
	    return false;

    Vector<const char *> state, nextstate;
    state.push_back(pattern.data());

    for (const char *s = str.data(); s != send && state.size(); ++s) {
	nextstate.clear();
	for (const char **pp = state.begin(); pp != state.end(); ++pp)
	    if (*pp != pend) {
	      reswitch:
		switch (**pp) {
		  case '?':
		    nextstate.push_back(*pp + 1);
		    break;
		  case '*':
		    if (*pp + 1 == pend)
			return true;
		    nextstate.push_back(*pp);
		    ++*pp;
		    goto reswitch;
		  case '\\':
		    if (*pp + 1 != pend)
			++*pp;
		    goto normal_char;
		  case '[': {
		      const char *ec = *pp + 1;
		      bool negated;
		      if (ec != pend && *ec == '^') {
			  negated = true;
			  ++ec;
		      } else
			  negated = false;
		      if (ec == pend)
			  goto normal_char;

		      bool found = false;
		      do {
			  if (*++ec == *s)
			      found = true;
		      } while (ec != pend && *ec != ']');
		      if (ec == pend)
			  goto normal_char;

		      if (found == !negated)
			  nextstate.push_back(ec + 1);
		      break;
		  }
		  normal_char:
		  default:
		    if (**pp == *s)
			nextstate.push_back(*pp + 1);
		    break;
		}
	    }
	state.swap(nextstate);
    }

    for (const char **pp = state.begin(); pp != state.end(); ++pp) {
	while (*pp != pend && **pp == '*')
	    ++*pp;
	if (*pp == pend)
	    return true;
    }
    return false;
}

void
click_signal(int signum, void (*handler)(int), bool resethand)
{
#if HAVE_SIGACTION
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = (resethand ? SA_RESETHAND : 0);
    sigaction(signum, &sa, 0);
#else
    signal(signum, handler);
    (void) resethand;
#endif
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
    const char *ad = a.begin(), *ae = a.end();
    const char *bd = b.begin(), *be = b.end();
    int raw_compare = 0;

    while (ad < ae && bd < be) {
	if ((isdigit((unsigned char) *ad) || *ad == '.')
	    && (isdigit((unsigned char) *bd) || *bd == '.')) {
	    // compare the two numbers, but treat them as strings
	    // (a decimal conversion might cause overflow)

	    // check if both are negative (note that if we get here, entire
	    // string prefixes are identical)
	    bool negative = false;
	    if (a.begin() < ad && ad[-1] == '-'
		&& (a.begin() == ad - 1 || isspace((unsigned char) ad[-2])))
		negative = true;

	    // skip initial '0's, but remember any difference in length
	    const char *iad = ad, *ibd = bd;
	    while (ad < ae && *ad == '0')
		++ad;
	    while (bd < be && *bd == '0')
		++bd;
	    int longer_zeros = (ad - iad) - (bd - ibd);

	    // walk over digits, remembering first nonidentical digit comparison
	    int digit_compare = 0;
	    bool a_good, b_good;
	    while (1) {
		a_good = ad < ae && isdigit((unsigned char) *ad);
		b_good = bd < be && isdigit((unsigned char) *bd);
		if (!a_good || !b_good)
		    break;
		if (digit_compare == 0)
		    digit_compare = *ad - *bd;
		++ad;
		++bd;
	    }

	    // if one number is longer, it must also be larger
	    if (a_good != b_good)
		return negative == a_good ? -1 : 1;
	    // otherwise, digit comparisons take precedence
	    if (digit_compare)
		return negative == (digit_compare > 0) ? -1 : 1;

	    // otherwise, integer parts are equal; check for fractions and
	    // compare them digit by digit
	    a_good = ad+1 < ae && *ad == '.' && isdigit((unsigned char) ad[1]);
	    b_good = bd+1 < be && *bd == '.' && isdigit((unsigned char) bd[1]);
	    if (a_good && b_good) {
		// inside fractions, the earliest digit difference wins
		++ad, ++bd;
		do {
		    if (*ad != *bd)
			return negative == (*ad > *bd) ? -1 : 1;
		    ++ad, ++bd;
		    a_good = ad < ae && isdigit((unsigned char) *ad);
		    b_good = bd < be && isdigit((unsigned char) *bd);
		} while (a_good && b_good);
		// then longer strings are greater; fallthru dtrt
	    }
	    if (a_good || b_good)
		return negative == a_good ? -1 : 1;

	    // as a last resort, the longer string of zeros is greater
	    if (longer_zeros)
		return longer_zeros;

	    // if we get here, the numeric portions were byte-for-byte
	    // identical; move on
	} else if (isdigit((unsigned char) *ad))
	    return isalpha((unsigned char) *bd) ? -1 : 1;
	else if (isdigit((unsigned char) *bd))
	    return isalpha((unsigned char) *ad) ? 1 : -1;
	else {
	    int alower = (unsigned char) tolower((unsigned char) *ad);
	    int blower = (unsigned char) tolower((unsigned char) *bd);
	    if (alower != blower)
		return alower - blower;
	    if (raw_compare == 0)
		raw_compare = (unsigned char) *ad - (unsigned char) *bd;
	    ++ad;
	    ++bd;
	}
    }

    if ((ae - ad) != (be - bd))
	return (ae - ad) - (be - bd);
    else
	return raw_compare;
}

const char *
filename_landmark(const char *filename, bool file_is_expr)
{
    if (file_is_expr)
	return "config";
    else if (!filename || !*filename || strcmp(filename, "-") == 0)
	return "<stdin>";
    else
	return filename;
}

String
shell_quote(const String &str, bool quote_tilde)
{
    StringAccum sa;
    const char *s, *last = str.begin();
    if (quote_tilde && str && str[0] == '~')
	sa << '\'';
    for (s = str.begin(); s < str.end(); s++) {
	if (isalnum((unsigned char) *s) || *s == '.' || *s == '/' || *s == '-'
	    || *s == '_' || *s == ',' || *s == '~')
	    /* do nothing */;
	else {
	    if (!sa)
		sa << str.substring(last, s) << '\'';
	    else
		sa << str.substring(last, s);
	    if (*s == '\'')
		sa << "\'\"\'\"\'";
	    else
		sa << *s;
	    last = s + 1;
	}
    }
    if (!sa)
	return str;
    else {
	sa << str.substring(last, s) << '\'';
	return sa.take_string();
    }
}

String
shell_command_output_string(String cmdline, const String &input, ErrorHandler *errh)
{
    FILE *f = tmpfile();
    if (!f) {
	errh->fatal("cannot create temporary file: %s", strerror(errno));
	return String();
    }
    ignore_result(fwrite(input.data(), 1, input.length(), f));
    fflush(f);
    rewind(f);

    String new_cmdline = cmdline + " <&" + String(fileno(f));
    FILE *p = popen(new_cmdline.c_str(), "r");
    if (!p) {
	errh->fatal("%<%s%>: %s", cmdline.c_str(), strerror(errno));
	return String();
    }

  StringAccum sa;
  while (!feof(p)) {
    if (char *s = sa.reserve(2048)) {
      int x = fread(s, 1, 2048, p);
      if (x > 0)
	sa.adjust_length(x);
    } else /* out of memory */
      break;
  }
  if (!feof(p))
    errh->warning("%<%s%> output too long, truncated", cmdline.c_str());

  fclose(f);
  pclose(p);
  return sa.take_string();
}

String
file_string(FILE *f, ErrorHandler *errh)
{
  StringAccum sa;
  while (!feof(f))
    if (char *x = sa.reserve(4096)) {
      size_t r = fread(x, 1, 4096, f);
      sa.adjust_length(r);
    } else {
      if (errh)
	errh->error("file too large, out of memory");
      errno = ENOMEM;
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
	    click_signal(SIGINT, signal_handler, false);
	    click_signal(SIGTERM, signal_handler, false);
	    click_signal(SIGPIPE, signal_handler, false);
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
	    errh->fatal("file %<%s%> not found\nin CLICKPATH %<%s%>", filename.c_str(), path);
	} else if (!path) {
	    // CLICKPATH not set
	    errh->fatal("file %<%s%> not found\nin install directory %<%s%>\n(Try setting the CLICKPATH environment variable.)", filename.c_str(), was_default_path.c_str());
	} else {
	    // CLICKPATH set, left opportunity to use default pathb
	    errh->fatal("file %<%s%> not found\nin CLICKPATH or %<%s%>", filename.c_str(), was_default_path.c_str());
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
    // check for gzip signatures
    if (len >= 3 && buf[0] == 037 && (buf[1] == 0235 || buf[1] == 0213))
	return true;
    // check for bzip2 signatures
    if (len >= 5 && buf[0] == 'B' && buf[1] == 'Z' && buf[2] == 'h'
	&& buf[3] >= '0' && buf[3] <= '9') {
	if (buf[4] == 0x17)	// compressed empty file
	    return true;
	if (len >= 10 && memcmp(buf + 4, "1AY&SY", 6) == 0)
	    return true;
    }
    // otherwise unknown
    return false;
}

FILE *
open_uncompress_pipe(const String &filename, const unsigned char *buf, int len, ErrorHandler *errh)
{
    assert(len >= 1);
    StringAccum cmd;
    if (buf[0] == 'B')
	cmd << "bzcat";
    else if (access("/usr/bin/gzcat", X_OK) >= 0)
	cmd << "/usr/bin/gzcat";
    else
	cmd << "zcat";
    cmd << ' ' << shell_quote(filename);
    if (FILE *p = popen(cmd.c_str(), "r"))
	return p;
    else {
	errh->error("%<%s%>: %s", cmd.c_str(), strerror(errno));
	return 0;
    }
}

enum {
    COMP_COMPRESS = 1, COMP_GZIP = 2, COMP_BZ2 = 3
};

int
compressed_filename(const String &filename)
{
    if (filename.length() >= 2 && memcmp(filename.end() - 2, ".Z", 2) == 0)
	return COMP_COMPRESS;
    if (filename.length() >= 3 && memcmp(filename.end() - 3, ".gz", 3) == 0)
	return COMP_GZIP;
    else if (filename.length() >= 4 && memcmp(filename.end() - 4, ".bz2", 4) == 0)
	return COMP_BZ2;
    else
	return 0;
}

FILE *
open_compress_pipe(const String &filename, ErrorHandler *errh)
{
    StringAccum cmd;
    int c = compressed_filename(filename);
    switch (c) {
      case COMP_COMPRESS:
	cmd << "compress";
	break;
      case COMP_GZIP:
	cmd << "gzip";
	break;
      case COMP_BZ2:
	cmd << "bzip2";
	break;
      default:
	errh->error("%s: unknown compression extension", filename.c_str());
	errno = EINVAL;
	return 0;
    }
    cmd << " > " << shell_quote(filename);
    if (FILE *p = popen(cmd.c_str(), "w"))
	return p;
    else {
	errh->error("%<%s%>: %s", cmd.c_str(), strerror(errno));
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
    return errh->error("package %<%s%> has no %<init_module%>", package.c_str());
  init_module_func init_func = (init_module_func)init_sym;
  if ((*init_func)() != 0)
    return errh->error("error initializing package %<%s%>", package.c_str());
  return 0;
}

#endif

CLICK_ENDDECLS

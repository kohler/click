/*
 * click-install.cc -- configuration installer for Click kernel module
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include "error.hh"
#include "confparse.hh"
#include "clp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 'h', HELP_OPT, 0, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-install' installs a Click configuration into the current Linux kernel.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -h, --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static String
path_find_file_2(const String &filename, String path,
		 String default_path)
{
  while (1) {
    int colon = path.find_left(':');
    String dir = (colon < 0 ? path : path.substring(0, colon));
    if (!dir && default_path) {
      String s = path_find_file_2(filename, default_path, String());
      if (s) return s;
      default_path = String();	// don't search default path twice
    } else if (dir) {
      String name = (dir[dir.length()-1] == '/' ? dir + filename : dir + "/" + filename);
      struct stat s;
      if (stat(name.cc(), &s) >= 0)
	return name;
    }
    if (colon < 0) return String();
    path = path.substring(colon + 1);
  }
}

static String
path_find_file(const String &filename, const char *path_variable,
	       const String &default_path)
{
  const char *path = getenv(path_variable);
  if (!path)
    return path_find_file_2(filename, default_path, "");
  else
    return path_find_file_2(filename, path, default_path);
}


static void
read_packages(HashMap<String, int> &packages, ErrorHandler *errh)
{
  packages.clear();
  FILE *f = fopen("/proc/click/packages", "r");
  if (!f)
    errh->warning("cannot read /proc/click/packages: %s", strerror(errno));
  else {
    char buf[1024];
    while (fgets(buf, 1024, f)) {
      String p = buf;
      if (p[p.length()-1] != '\n')
	errh->warning("/proc/click/packages: line too long");
      else
	packages.insert(p.substring(0, -1), 0);
    }
    fclose(f);
  }
}

RouterT *
read_router_file(const char *filename, ErrorHandler *errh)
{
  FILE *f;
  if (filename && strcmp(filename, "-") != 0) {
    f = fopen(filename, "r");
    if (!f) {
      errh->error("%s: %s", filename, strerror(errno));
      return 0;
    }
  } else {
    f = stdin;
    filename = "<stdin>";
  }
  
  FileLexerTSource lex_source(filename, f);
  LexerT lexer(errh);
  lexer.reset(&lex_source);
  while (lexer.ystatement()) ;
  RouterT *r = lexer.take_router();
  
  if (f != stdin) fclose(f);
  return r;
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-install: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-install (Click) %s\n", VERSION);
      printf("Copyright (C) 1999 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
     case Clp_NotOption:
      if (router_file) {
	errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;

     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  RouterT *r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  
  r->flatten(errh);

  // check for Click module; install it if not available
  {
    struct stat s;
    if (stat("/proc/click", &s) < 0) {
      // try to install module
      String click_o = path_find_file("click.o", "CLICK_LIB", CLICK_LIBDIR);
      if (!click_o) {
	errh->message("cannot find Click module `click.o'");
	errh->fatal("in CLICK_LIB or `%s'", CLICK_LIBDIR);
      }
      String cmdline = "/sbin/insmod " + click_o;
      (void) system(cmdline);
      if (stat("/proc/click", &s) < 0)
	errh->fatal("cannot install Click module");
    }
  }

  // find current packages
  HashMap<String, int> packages(-1);
  read_packages(packages, errh);
  
  // install missing requirements
  {
    const HashMap<String, int> &requirements = r->requirement_map();
    int thunk = 0, value; String key;
    while (requirements.each(thunk, key, value))
      if (value >= 0 && packages[key] < 0) {
	String package = path_find_file
	  (key + ".o", "CLICK_PACKAGES", CLICK_PACKAGESDIR);
	if (!package) {
	  errh->message("cannot find required package `%s.o'", key.cc());
	  errh->fatal("in CLICK_PACKAGES or `%s'", CLICK_PACKAGESDIR);
	}
	String cmdline = "/sbin/insmod " + package;
	int retval = system(cmdline);
	if (retval != 0)
	  errh->fatal("`insmod %s' failed: %s", package.cc(), strerror(errno));
      }
  }
  
  // write flattened configuration to /proc/click/config
  FILE *f = fopen("/proc/click/config", "w");
  if (!f)
    errh->fatal("cannot install configuration: %s", strerror(errno));
  String s = r->configuration_string();
  fputs(s.cc(), f);
  fclose(f);

  // report errors
  {
    char buf[1024];
    FILE *f = fopen("/proc/click/errors", "r");
    if (!f)
      errh->warning("cannot read /proc/click/errors: %s", strerror(errno));
    else {
      while (!feof(f)) {
	size_t s = fread(buf, 1, 1024, f);
	fwrite(buf, 1, s, stderr);
      }
      fclose(f);
    }
  }

  // remove unused packages
  {
    read_packages(packages, errh);
    const HashMap<String, int> &requirements = r->requirement_map();
    int thunk = 0, value; String key;
    String to_remove;
    while (packages.each(thunk, key, value))
      if (value >= 0 && requirements[key] < 0)
	to_remove += " " + key;
    if (to_remove) {
      String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
      (void) system(cmdline);
    }
  }
  
  return 0;
}

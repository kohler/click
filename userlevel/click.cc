/*
 * click.cc -- user-level Click main program
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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>

#include "lexer.hh"
#include "router.hh"
#include "error.hh"
#include "timer.hh"
#include "straccum.hh"
#include "clp.h"
#include "archive.hh"
#include "glue.hh"
#include "clickpackage.hh"

#if defined(HAVE_DLFCN_H) && defined(HAVE_LIBDL)
# define HAVE_DYNAMIC_LINKING 1
# include <dlfcn.h>
#endif

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define QUIT_OPT		303
#define OUTPUT_OPT		304
#define HANDLER_OPT		305
#define TIME_OPT		306

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "handler", 'h', HANDLER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "quit", 'q', QUIT_OPT, 0, 0 },
  { "time", 't', TIME_OPT, 0, 0 },
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
`Click' runs a Click router configuration at user level. It installs the\n\
configuration, reporting any errors to standard error, and then generally runs\n\
until interrupted.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -h, --handler ELEMENT.H       Call ELEMENT's read handler H after running\n\
                                driver and print result to standard output.\n\
  -o, --output FILE             Write flat configuration to FILE.\n\
  -q, --quit                    Do not run driver.\n\
  -t, --time                    Print information on how long driver took.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
catch_sigint(int)
{
  /* call exit so -pg file is written */
  exit(0);
}


// stuff for dynamic linking

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
      return name;
    } else if (errno != EEXIST) {
      errh->error("cannot create temporary file: %s", strerror(errno));
      return String();
    }
    uniqueifier++;
  }
}

#if HAVE_DYNAMIC_LINKING
extern "C" {
typedef int (*init_module_func)(void);
}

static int
load_package(String package, ErrorHandler *errh)
{
#ifndef RTLD_NOW
  void *handle = dlopen((char *)package.cc(), RTLD_LAZY);
#else
  void *handle = dlopen((char *)package.cc(), RTLD_NOW);
#endif
  if (!handle)
    return errh->error("cannot load package: %s", dlerror());
  void *init_sym = dlsym(handle, "init_module");
  if (!init_sym)
    return errh->error("package `%s' has no `init_module'", package.cc());
  init_module_func init_func = (init_module_func)init_sym;
  if (init_func() != 0)
    return errh->error("error initializing package `%s'", package.cc());
  return 0;
}
#endif


// functions for packages

static Lexer *lexer;
static Vector<String> packages;

extern "C" void
click_provide(const char *package)
{
  packages.push_back(package);
}

extern "C" void
click_unprovide(const char *package)
{
  String s = package;
  for (int i = 0; i < packages.size(); i++)
    if (packages[i] == s) {
      packages[i] = packages.back();
      packages.pop_back();
      return;
    }
}

extern "C" int
click_add_element_type(const char *ename, Element *e)
{
  int c = lexer->add_element_type(ename, e);
  lexer->save_element_types();
  return c;
}

extern "C" void
click_remove_element_type(int which)
{
  lexer->remove_element_type(which);
}


// register handlers

struct Handler {
  Element *element;
  String name;
  ReadHandler read;
  void *read_thunk;
  WriteHandler write;
  void *write_thunk;
};

static Vector<Handler> handlers;

class UserHandlerRegistry : public Element::HandlerRegistry {
  
  Element *_element;
  
 public:
  
  UserHandlerRegistry() : _element(0) { }
  void set_element(Element *e) { _element = e; }
  void add_read_write(const char *, int, ReadHandler, void *,
		      WriteHandler, void *);
  
};

void
UserHandlerRegistry::add_read_write(const char *n, int l, ReadHandler r,
				    void *rt, WriteHandler w, void *wt)
{
  Handler h;
  h.element = _element; h.name = String(n, l);
  h.read = r; h.read_thunk = rt; h.write = w; h.write_thunk = wt;
  handlers.push_back(h);
}

static int
call_read_handler(String s, Router *r, bool print_name, ErrorHandler *errh)
{
  int dot = s.find_left('.');
  if (dot < 0)
    return errh->error("bad read handler syntax: expected ELEMENTNAME.HANDLERNAME");
  String element_name = s.substring(0, dot);
  String handler_name = s.substring(dot + 1);
  
  Element *e = r->find(element_name, errh);
  if (!e)
    return -1;

  for (int i = 0; i < handlers.size(); i++)
    if (handlers[i].element == e && handlers[i].name == handler_name) {
      if (!handlers[i].read)
	return errh->error("`%s' is a write handler", s.cc());
      String result = handlers[i].read(e, handlers[i].read_thunk);
      if (print_name)
	fprintf(stdout, "%s:\n", s.cc());
      fputs(result.cc(), stdout);
      if (print_name)
	fputs("\n", stdout);
      return 0;
    }

  return errh->error("no `%s' handler for element `%s'", handler_name.cc(), element_name.cc());
}

// main

extern void export_elements(Lexer *);

int
main(int argc, char **argv)
{
  String::static_initialize();
  Timer::static_initialize();
  ErrorHandler *errh = new FileErrorHandler(stderr, "");
  ErrorHandler::static_initialize(errh);

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool quit_immediately = false;
  bool report_time = false;
  Vector<String> call_handlers;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case Clp_NotOption:
     case ROUTER_OPT:
      if (router_file) {
	errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case HANDLER_OPT:
      call_handlers.push_back(clp->arg);
      break;
      
     case QUIT_OPT:
      quit_immediately = true;
      break;

     case TIME_OPT:
      report_time = true;
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click (Click) %s\n", VERSION);
      printf("Copyright (C) 1999 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
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
  FILE *f;
  String filename;
  if (router_file && strcmp(router_file, "-") != 0) {
    f = fopen(router_file, "r");
    if (!f)
      errh->fatal("%s: %s", router_file, strerror(errno));
    filename = router_file;
  } else {
    f = stdin;
    filename = "<stdin>";
  }

  // read string from file
  StringAccum config_sa;
  while (!feof(f)) {
    if (char *x = config_sa.reserve(2048)) {
      int r = fread(x, 1, 2048, f);
      config_sa.forward(r);
    } else
      errh->fatal("out of memory");
  }
  if (f != stdin)
    fclose(f);

  // prepare lexer (for packages)
  lexer = new Lexer(errh);
  export_elements(lexer);
  lexer->save_element_types();
  
  // XXX locals should override
#if HAVE_DYNAMIC_LINKING
  int num_packages = 0;
#endif
  
  // find config string in archive
  String config_str = config_sa.take_string();
  if (config_str.length() != 0 && config_str[0] == '!') {
    Vector<ArchiveElement> archive;
    separate_ar_string(config_str, archive, errh);
    bool found_config = false;
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name == "config") {
	config_str = archive[i].data;
	found_config = true;
      }
#if HAVE_DYNAMIC_LINKING
      else if (HAVE_DYNAMIC_LINKING
	       && archive[i].name.length() > 3
	       && archive[i].name.substring(-3) == ".uo") {
	num_packages++;
	String tmpnam = unique_tmpnam("package" + String(num_packages) + "-*.uo", errh);
	if (!tmpnam) exit(1);
	FILE *f = fopen(tmpnam.cc(), "wb");
	fwrite(archive[i].data.data(), 1, archive[i].data.length(), f);
	fclose(f);
	int ok = load_package(tmpnam, errh);
	unlink(tmpnam.cc());
	if (ok < 0) exit(1);
      }
#endif
    if (!found_config) {
      errh->error("archive has no `config' section");
      config_str = String();
    }
  }

  // lex
  lexer->reset(config_str, filename);
  while (lexer->ystatement())
    /* do nothing */;
  
  Router *router = lexer->create_router();
  lexer->clear();
  
  signal(SIGINT, catch_sigint);

  if (router->initialize(errh) < 0)
    exit(1);

  int exit_value = 0;

  // output flat configuration
  if (output_file) {
    FILE *f = 0;
    if (strcmp(output_file, "-") != 0) {
      f = fopen(output_file, "w");
      if (!f) {
	errh->error("%s: %s", output_file, strerror(errno));
	exit_value = 1;
      }
    } else
      f = stdout;
    if (f) {
      String s = router->flat_configuration_string();
      fputs(s.cc(), f);
      if (f != stdout) fclose(f);
    }
  }

  struct rusage before, after;
  struct timeval before_time, after_time;
  getrusage(RUSAGE_SELF, &before);
  gettimeofday(&before_time, 0);

  // run driver
  if (!quit_immediately) {
    while (router->driver())
      /* nada */;
  }
  
  gettimeofday(&after_time, 0);
  getrusage(RUSAGE_SELF, &after);
  // report time
  if (report_time) {
    struct timeval diff;
    timersub(&after.ru_utime, &before.ru_utime, &diff);
    printf("%ld.%03ldu", diff.tv_sec, (diff.tv_usec+500)/1000);
    timersub(&after.ru_stime, &before.ru_stime, &diff);
    printf(" %ld.%03lds", diff.tv_sec, (diff.tv_usec+500)/1000);
    timersub(&after_time, &before_time, &diff);
    printf(" %ld:%02ld.%02ld", diff.tv_sec/60, diff.tv_sec%60,
	   (diff.tv_usec+5000)/10000);
    printf("\n");
  }
  
  // call handlers
  if (call_handlers.size()) {
    int nelements = router->nelements();
    UserHandlerRegistry uhr;
    for (int i = 0; i < nelements; i++) {
      Element *e = router->element(i);
      uhr.set_element(e);
      e->add_default_handlers(&uhr, false);
      e->add_handlers(&uhr);
    }
    int nerrors = errh->nerrors();
    for (int i = 0; i < call_handlers.size(); i++)
      call_read_handler(call_handlers[i], router, call_handlers.size() > 1,
			errh);
    if (nerrors != errh->nerrors())
      exit_value = 1;
  }

  delete router;
  delete lexer;
  exit(exit_value);
}

// generate Vector template instance
#include "vector.cc"
template class Vector<Handler>;

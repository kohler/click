/*
 * click.cc -- user-level Click main program
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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
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
#include "userutils.hh"
#include "elements/userlevel/readhandler.hh"

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
#define DURATION_OPT		307
#define DIR_OPT			308

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "handler", 'h', HANDLER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "duration", 'u', DURATION_OPT, Clp_ArgUnsigned, 0 },
  { "directory", 'd', DIR_OPT, Clp_ArgString, 0 },
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
  -u, --duration DURATION	Call specified read handlers once every \n\
                                DURATION number of seconds while running. \n\
  -d, --directory DIR		Write output of read handlers into DIR. \n\
  -o, --output FILE             Write flat configuration to FILE.\n\
  -q, --quit                    Do not run driver.\n\
  -t, --time                    Print information on how long driver took.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Router *router;
static Vector<String> call_handlers;
static ErrorHandler *errh;
static int handler_duration = -1;
static const char *handler_dir = 0;
static ReadHandlerCaller* readhandler_element = 0;

static void
catch_sigint(int)
{
  /* call exit so -pg file is written */
  // exit(0);
  router->please_stop_driver();
}


// stuff for dynamic linking

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
  return lexer->add_element_type(ename, e);
}

extern "C" void
click_remove_element_type(int which)
{
  lexer->remove_element_type(which);
}


// register handlers: this gets called in signal handler, so don't use
// new/malloc

static int
call_read_handler(String s, Router *r, bool print_name, const char *handler_dir, ErrorHandler *errh)
{
  int dot = s.find_left('.');
  if (dot < 0)
    return errh->error("bad read handler syntax: expected ELEMENTNAME.HANDLERNAME");
  String element_name = s.substring(0, dot);
  String handler_name = s.substring(dot + 1);
  
  Element *e = r->find(element_name, errh);
  if (!e)
    return -1;
  
  int hi = r->find_handler(e, handler_name);
  if (hi < 0)
    return errh->error("no `%s' handler for element `%s'", handler_name.cc(), element_name.cc());
  
  const Router::Handler &rh = r->handler(hi);
  if (!rh.read)
    return errh->error("`%s' is a write handler", s.cc());
  String result = rh.read(e, rh.read_thunk);

  bool tofile = true;
  char fpath[MAXPATHLEN];
  char fdir[MAXPATHLEN];
  FILE *fout = 0;
  if (handler_dir && (strlen(handler_dir)+strlen(element_name.cc())+
                      strlen(handler_name.cc())+2) < MAXPATHLEN) {
    sprintf(fdir,"%s/%s", handler_dir,element_name.cc());
    sprintf(fpath,"%s/%s/%s", handler_dir,element_name.cc(),handler_name.cc());
    if ((mkdir(fdir, S_IRWXU) < 0 && errno != EEXIST)
	|| (fout = fopen(fpath,"w")) == 0L) fout = 0;
  }
  if (!fout) {
    fout = stdout;
    tofile = false;
  }

  if (print_name && !tofile)
    fprintf(fout, "%s:\n", s.cc());
  fputs(result.cc(), fout);
  if (print_name && !tofile)
    fputs("\n", fout);

  if (tofile)
    fclose(fout);
  return 0;
}


// compile packages

#if HAVE_DYNAMIC_LINKING

static bool
prepare_compile_tmpdir(Vector<ArchiveElement> &archive,
		       String &tmpdir, String &compile_prog,
		       ErrorHandler *errh)
{
  ContextErrorHandler cerrh(errh, "While preparing to compile packages:");
  
  // change to temporary directory
  tmpdir = click_mktmpdir(&cerrh);
  if (!tmpdir)
    return false;
  if (chdir(tmpdir.cc()) < 0)
    cerrh.fatal("cannot chdir to %s: %s", tmpdir.cc(), strerror(errno));

  // find compile program
  compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, &cerrh);
  if (!compile_prog)
    return false;

  // look for .hh files
  for (int i = 0; i < archive.size(); i++)
    if (archive[i].name.substring(-3) == ".hh") {
      String filename = archive[i].name;
      FILE *f = fopen(filename, "w");
      if (!f)
	cerrh.warning("%s: %s", filename.cc(), strerror(errno));
      else {
	fwrite(archive[i].data.data(), 1, archive[i].data.length(), f);
	fclose(f);
      }
    }

  return true;
}

static void
compile_archive_packages(Vector<ArchiveElement> &archive,
			 ErrorHandler *errh)
{
  HashMap<String, int> have_requirements(-1);

  // analyze archive
  for (int i = 0; i < archive.size(); i++) {
    const ArchiveElement &ae = archive[i];
    if (ae.name.substring(-3) == ".cc") {
      int &have = have_requirements.find_force(ae.name.substring(0, -3));
      if (have == -1)
	have = i;
    } else if (ae.name.substring(-3) == ".uo") {
      int &have = have_requirements.find_force(ae.name.substring(0, -3));
      have = -2;
    }
  }
  
  String tmpdir;
  String click_compile_prog;
  
  // check requirements
  int thunk = 0, value; String package;
  while (have_requirements.each(thunk, package, value))
    if (value >= 0) {
      // have source, but not package; compile it
      // XXX what if it's not required?
      
      if (!click_compile_prog)
	if (!prepare_compile_tmpdir(archive, tmpdir, click_compile_prog, errh))
	  exit(1);

      ContextErrorHandler cerrh
	(errh, "While compiling package `" + package + ".uo':");

      // write .cc file
      String filename = package + ".cc";
      String source_text = archive[value].data;
      FILE *f = fopen(filename, "w");
      if (!f)
	cerrh.fatal("%s: %s", filename.cc(), strerror(errno));
      fwrite(source_text.data(), 1, source_text.length(), f);
      fclose(f);
      // grab compiler options
      String compiler_options;
      if (source_text.substring(0, 17) == "// click-compile:") {
	const char *s = source_text.data();
	int pos = 17;
	int len = source_text.length();
	while (pos < len && s[pos] != '\n' && s[pos] != '\r')
	  pos++;
	// XXX check user input for shell metas?
	compiler_options = source_text.substring(17, pos - 17) + " ";
      }
      
      // run click-compile
      errh->message("Compiling `%s.uo'...", package.cc());
      String compile_command = click_compile_prog + " --target=user --package=" + package + ".uo " + compiler_options + filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	cerrh.fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	cerrh.fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	cerrh.fatal("`%s' failed", compile_command.cc());

      // grab object file and add to archive
      ArchiveElement ae = init_archive_element(package + ".uo", 0600);
      ae.data = file_string(package + ".uo", &cerrh);
      archive.push_back(ae);
    }
}

#endif
  
int call_read_handlers()
{
  // call handlers
  if (call_handlers.size()) {
    int nerrs = errh->nerrors();
    for (int i = 0; i < call_handlers.size(); i++)
      call_read_handler(call_handlers[i], router,
	                call_handlers.size() > 1, handler_dir, errh);
    if (errh->nerrors() > nerrs) return 1;
  }
  return 0;
}

static void
catch_sigalrm(int)
{
  click_chatter("signal alarm");
  if (readhandler_element) 
    readhandler_element->schedule_immediately();
  alarm(handler_duration);
}

// main

extern void export_elements(Lexer *);

int
main(int argc, char **argv)
{
  String::static_initialize();
  Element::static_initialize();
  
  errh = new FileErrorHandler(stderr, "");
  ErrorHandler::static_initialize(errh);

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool quit_immediately = false;
  bool report_time = false;
  
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
     
     case DIR_OPT:
      if (handler_dir) {
	errh->error("hander output directory specified twice");
	goto bad_option;
      }
      handler_dir = clp->arg;
      break;

     case DURATION_OPT:
      if (handler_duration > -1) {
	errh->error("duration specified twice");
	goto bad_option;
      }
      handler_duration = clp->val.u;
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
  String config_str = file_string(router_file, errh);
  if (errh->nerrors() > 0)
    exit(1);
  if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";

  // prepare lexer (for packages)
  lexer = new Lexer(errh);
  export_elements(lexer);
  
  // XXX locals should override
#if HAVE_DYNAMIC_LINKING
  int num_packages = 0;
#endif
  
  // find config string in archive
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
    compile_archive_packages(archive, errh);
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name.substring(-3) == ".uo") {
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
  int cookie = lexer->begin_parse(config_str, router_file, 0);
  while (lexer->ystatement())
    /* do nothing */;
  router = lexer->create_router();
  lexer->end_parse(cookie);
  
  signal(SIGINT, catch_sigint);
  signal(SIGALRM, catch_sigalrm);

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

  // register handlers
  int nelements = router->nelements();
  for (int i = 0; i < nelements; i++) {
    Element *e = router->element(i);
    e->add_default_handlers(false);
    e->add_handlers();
  }
  
  // create a ReadHandlerCaller element: is this kosher? we don't use
  // ReadHandlerCaller anywhere else...
  readhandler_element = new ReadHandlerCaller();
  readhandler_element->initialize_link(router);
  alarm(handler_duration);

  // run driver
  if (!quit_immediately)
    router->driver();
  
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
  int ev = 0;
  if ((ev = call_read_handlers()))
    exit_value = ev;

  delete readhandler_element;
  delete router;
  delete lexer;
  exit(exit_value);
}

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
#include "confparse.hh"
#include "elements/standard/quitwatcher.hh"
#include "elements/userlevel/controlsocket.hh"

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
#define STOP_OPT		307
#define PORT_OPT		308
#define UNIX_SOCKET_OPT		309

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "handler", 'h', HANDLER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "port", 'p', PORT_OPT, Clp_ArgInt, 0 },
  { "quit", 'q', QUIT_OPT, 0, 0 },
  { "stop", 's', STOP_OPT, Clp_ArgString, Clp_Optional },
  { "time", 't', TIME_OPT, 0, 0 },
  { "unix-socket", 'u', UNIX_SOCKET_OPT, Clp_ArgString, 0 },
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
  -p, --port PORT               Listen for control connections on TCP port.\n\
  -u, --unix-socket FILE        Listen for control connections on Unix socket.\n\
  -h, --handler ELEMENT.H       Call ELEMENT's read handler H after running\n\
                                driver and print result to standard output.\n\
  -o, --output FILE             Write flat configuration to FILE.\n\
  -q, --quit                    Do not run driver.\n\
  -s, --stop[=ELEMENT]          Stop driver once ELEMENT is done. Can be given\n\
                                multiple times. If not specified, ELEMENTS are\n\
                                the configuration's packet sources.\n\
  -t, --time                    Print information on how long driver took.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Router *router;
static ErrorHandler *errh;
static bool started = 0;

static void
catch_sigint(int)
{
  /* call exit so -pg file is written */
  // exit(0);
  signal(SIGINT, SIG_DFL);
  if (!started)
    kill(getpid(), SIGINT);
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

static String::Initializer crap_initializer;
static Lexer *lexer;
static Vector<String> packages;
static String configuration_string;

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


// global handlers for ControlSocket

String
click_userlevel_classes_string()
{
  Vector<String> v;
  lexer->element_type_names(v);
  StringAccum sa;
  for (int i = 0; i < v.size(); i++)
    sa << v[i] << "\n";
  return sa.take_string();
}

String
click_userlevel_config_string()
{
  return configuration_string;
}

String
click_userlevel_packages_string()
{
  StringAccum sa;
  for (int i = 0; i < packages.size(); i++)
    sa << packages[i] << "\n";
  return sa.take_string();
}


// report handler results

static int
call_read_handler(Element *e, String handler_name, Router *r,
		  bool print_name, ErrorHandler *errh)
{
  int hi = r->find_handler(e, handler_name);
  if (hi < 0)
    return errh->error("no `%s' handler for element `%s'", handler_name.cc(), e->id().cc());
  
  const Router::Handler &rh = r->handler(hi);
  if (!rh.read)
    return errh->error("`%s.%s' is a write handler", e->id().cc(), handler_name.cc());
  String result = rh.read(e, rh.read_thunk);

  if (print_name)
    fprintf(stdout, "%s.%s:\n", e->id().cc(), handler_name.cc());
  fputs(result.cc(), stdout);
  if (print_name)
    fputs("\n", stdout);

  return 0;
}

static void
expand_handler_elements(const String &pattern, const String &handler_name,
			Vector<Element *> &elements, Router *router)
{
  int nelem = router->nelements();
  for (int i = 0; i < nelem; i++) {
    const String &id = router->ename(i);
    if (glob_match(id, pattern)) {
      Element *e = router->element(i);
      int hi = router->find_handler(e, handler_name);
      if (hi >= 0 && router->handler(hi).read)
	elements.push_back(e);
    }
  }
}

static int
call_read_handlers(Vector<String> &handlers, ErrorHandler *errh)
{
  Vector<Element *> handler_elements;
  Vector<String> handler_names;
  bool print_names = (handlers.size() > 1);
  int before = errh->nerrors();

  // expand handler names
  for (int i = 0; i < handlers.size(); i++) {
    int dot = handlers[i].find_left('.');
    if (dot < 0) {
      errh->error("syntax error in handler `%s': expected ELEMENTNAME.HANDLERNAME", handlers[i].cc());
      continue;
    }
    
    String element_name = handlers[i].substring(0, dot);
    String handler_name = handlers[i].substring(dot + 1);

    Vector<Element *> elements;
    if (Element *e = router->find(element_name))
      elements.push_back(e);
    else {
      expand_handler_elements(element_name, handler_name, elements, router);
      print_names = true;
    }
    if (!elements.size()) {
      errh->error("no element named `%s'", element_name.cc());
      continue;
    }

    for (int j = 0; j < elements.size(); j++)
      call_read_handler(elements[j], handler_name, router, print_names, errh);
  }

  return (errh->nerrors() == before ? 0 : -1);
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
    if (ae.name.substring(-5) == ".u.cc") {
      int &have = have_requirements.find_force(ae.name.substring(0, -5));
      if (have == -1 || have >= 0) // prefer .u.cc to .cc
	have = i;
    } else if (ae.name.substring(-3) == ".cc" && (ae.name.length() < 5 || ae.name[ae.name.length()-5] != '.')) {
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
  for (HashMap<String, int>::Iterator iter = have_requirements.first();
       iter; iter++)
    if (iter.value() >= 0) {
      // have source, but not package; compile it
      // XXX what if it's not required?
      String package = iter.key();
      int archive_index = iter.value();
      
      if (!click_compile_prog)
	if (!prepare_compile_tmpdir(archive, tmpdir, click_compile_prog, errh))
	  exit(1);

      ContextErrorHandler cerrh
	(errh, "While compiling package `" + package + ".uo':");

      // write .cc file
      String filename = archive[archive_index].name;
      String source_text = archive[archive_index].data;
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
  bool stop = false;
  bool stop_guess = false;
  Vector<String> handlers;
  Vector<String> stops;
  Vector<String> unix_sockets;
  Vector<int> ports;
  
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
     
     case STOP_OPT:
      if (stop && ((stop_guess && clp->have_arg) || (!stop_guess && !clp->have_arg))) {
	errh->error("conflicting `--stop' options: guess or not?");
	goto bad_option;
      }
      stop = true;
      if (!clp->have_arg)
	stop_guess = true;
      else
	stops.push_back(clp->arg);
      break;
      
     case HANDLER_OPT:
      handlers.push_back(clp->arg);
      break;

     case PORT_OPT:
      ports.push_back(clp->val.i);
      break;

     case UNIX_SOCKET_OPT:
      unix_sockets.push_back(clp->arg);
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
      printf("Copyright (C) 1999-2000 Massachusetts Institute of Technology\n\
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

  // save config_str
  ::configuration_string = config_str;

  // lex
  int cookie = lexer->begin_parse(config_str, router_file, 0);
  while (lexer->ystatement())
    /* do nothing */;
  router = lexer->create_router();
  lexer->end_parse(cookie);

  if (router->nelements() == 0)
    errh->warning("%s: configuration has no elements", router_file);

  // handle stop option by adding a QuitWatcher element
  if (stop && stop_guess) {
    for (int i = 0; i < router->nelements(); i++) {
      Element *e = router->element(i);
      if (e->cast("InfiniteSource") || e->cast("RatedSource")
	  || e->cast("FromDump"))
	stops.push_back(e->id());
    }
  }
  if (stop && !stops.size())
    errh->error("`--stop' option given, but configuration has no packet sources");
  if (stop)
    router->add_element(new QuitWatcher, "click_driver@@QuitWatcher", cp_unargvec(stops), "click");

  // add new ControlSockets
  for (int i = 0; i < ports.size(); i++)
    router->add_element(new ControlSocket, "click_driver@@ControlSocket@" + String(i), "tcp, " + String(ports[i]), "click");
  for (int i = 0; i < unix_sockets.size(); i++)
    router->add_element(new ControlSocket, "click_driver@@ControlSocket@" + String(i + ports.size()), "unix, " + cp_quote(unix_sockets[i]), "click");

  // catch control-C
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

  // register handlers
  int nelements = router->nelements();
  for (int i = 0; i < nelements; i++) {
    Element *e = router->element(i);
    e->add_default_handlers(false);
    e->add_handlers();
  }

  // run driver
  if (!quit_immediately) {
    started = true;
    router->driver();
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
  if (handlers.size())
    if (call_read_handlers(handlers, errh) < 0)
      exit_value = 1;

  delete router;
  delete lexer;
  exit(exit_value);
}

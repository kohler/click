/*
 * click.cc -- user-level Click main program
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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

#include <cstdio>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>

#include <click/lexer.hh>
#include <click/routerthread.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <click/clp.h>
#include <click/archive.hh>
#include <click/glue.hh>
#include <click/driver.hh>
#include <click/userutils.hh>
#include <click/confparse.hh>
#include "elements/standard/quitwatcher.hh"
#include "elements/userlevel/controlsocket.hh"
CLICK_USING_DECLS

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define QUIT_OPT		305
#define OUTPUT_OPT		306
#define HANDLER_OPT		307
#define TIME_OPT		308
#define STOP_OPT		309
#define PORT_OPT		310
#define UNIX_SOCKET_OPT		311
#define NO_WARNINGS_OPT		312
#define WARNINGS_OPT		313

static Clp_Option options[] = {
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
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
  { "warnings", 0, WARNINGS_OPT, 0, Clp_Negate },
  { 0, 'w', NO_WARNINGS_OPT, 0, Clp_Negate },
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
  -e, --expression EXPR         Use EXPR as router configuration.\n\
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
  -w, --no-warnings             Do not print warnings.\n\
  -C, --clickpath PATH          Use PATH for CLICKPATH.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Router *router;
static ErrorHandler *errh;
static bool started = 0;

extern "C" {
static void
catch_signal(int sig)
{
  signal(sig, SIG_DFL);
  if (!started)
    kill(getpid(), sig);
  router->please_stop_driver();
}
}


// functions for packages

static String::Initializer crap_initializer;
static Lexer *lexer;
static String configuration_string;
static Vector<ArchiveElement> archive;

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

enum {
  GH_VERSION, GH_LIST, GH_CLASSES, GH_CONFIG,
  GH_FLATCONFIG, GH_PACKAGES, GH_REQUIREMENTS
};

String
read_global_handler(Element *, void *thunk)
{
  StringAccum sa;

  switch (reinterpret_cast<int>(thunk)) {

   case GH_VERSION:
    return String(CLICK_VERSION "\n");

   case GH_LIST:
    return router->element_list_string();
  
   case GH_CLASSES: {
     Vector<String> v;
     lexer->element_type_names(v);
     for (int i = 0; i < v.size(); i++)
       sa << v[i] << "\n";
     return sa.take_string();
   }

   case GH_CONFIG:
    return configuration_string;

   case GH_FLATCONFIG:
    return router->flat_configuration_string();

   case GH_PACKAGES: {
     Vector<String> p;
     click_public_packages(p);
     for (int i = 0; i < p.size(); i++)
       sa << p[i] << "\n";
     return sa.take_string();
   }

   case GH_REQUIREMENTS: {
     const Vector<String> &v = router->requirements();
     for (int i = 0; i < v.size(); i++)
       sa << v[i] << "\n";
     return sa.take_string();
   }

   default:
    return "<error>\n";

  }
}

int
stop_global_handler(const String &s, Element *, void *, ErrorHandler *)
{
  int n = 1;
  (void) cp_integer(cp_uncomment(s), &n);
  router->adjust_driver_reservations(-n);
  return 0;
}


// report handler results

static int
call_read_handler(Element *e, String handler_name, Router *r,
		  bool print_name, ErrorHandler *errh)
{
  int hi = r->find_handler(e, handler_name);
  if (hi < 0)
    return errh->error("no `%s' handler", Router::Handler::unparse_name(e, handler_name).cc());

  const Router::Handler &rh = r->handler(hi);
  String full_name = rh.unparse_name(e);
  
  if (!rh.visible())
    return errh->error("no `%s' handler", full_name.cc());
  else if (!rh.read_visible())
    return errh->error("`%s' is a write handler", full_name.cc());
  String result = rh.call_read(e);

  if (print_name)
    fprintf(stdout, "%s:\n", full_name.cc());
  fputs(result.cc(), stdout);
  if (print_name)
    fputs("\n", stdout);

  return 0;
}

static bool
expand_handler_elements(const String &pattern, const String &handler_name,
			Vector<Element *> &elements, Router *router)
{
  int nelem = router->nelements();
  bool any_elements = false;
  for (int i = 0; i < nelem; i++) {
    const String &id = router->ename(i);
    if (glob_match(id, pattern)) {
      any_elements = true;
      Element *e = router->element(i);
      int hi = router->find_handler(e, handler_name);
      if (hi >= 0 && router->handler(hi).read_visible())
	elements.push_back(e);
    }
  }
  return any_elements;
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
      call_read_handler(0, handlers[i], router, print_names, errh);
      continue;
    }
    
    String element_name = handlers[i].substring(0, dot);
    String handler_name = handlers[i].substring(dot + 1);

    Vector<Element *> elements;
    if (Element *e = router->find(element_name))
      elements.push_back(e);
    else if (expand_handler_elements(element_name, handler_name, elements, router))
      print_names = true;
    else
      errh->error("no element matching `%s'", element_name.cc());

    for (int j = 0; j < elements.size(); j++)
      call_read_handler(elements[j], handler_name, router, print_names, errh);
  }

  return (errh->nerrors() == before ? 0 : -1);
}


// include requirements

class RequireLexerExtra : public LexerExtra { public:

  RequireLexerExtra()			{ }

  void require(String, ErrorHandler *);
  
};

void
RequireLexerExtra::require(String name, ErrorHandler *errh)
{
#ifdef HAVE_DYNAMIC_LINKING
  if (!click_has_provision(name))
    clickdl_load_requirement(name, &archive, errh);
#endif
  if (!click_has_provision(name))
    errh->error("requirement `%s' not available", name.cc());
}


// main

extern void export_elements(Lexer *);

int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();

  errh = new FileErrorHandler(stderr, "");
  ErrorHandler::static_initialize(errh);
  
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  bool file_is_expr = false;
  const char *output_file = 0;
  bool quit_immediately = false;
  bool report_time = false;
  bool stop = false;
  bool stop_guess = false;
  bool warnings = true;
  Vector<String> handlers;
  Vector<String> stops;
  Vector<String> unix_sockets;
  Vector<int> ports;

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case ROUTER_OPT:
     case EXPRESSION_OPT:
     case Clp_NotOption:
      if (router_file) {
	errh->error("router configuration specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      file_is_expr = (opt == EXPRESSION_OPT);
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

     case WARNINGS_OPT:
      warnings = !clp->negated;
      break;

     case NO_WARNINGS_OPT:
      warnings = clp->negated;
      break;

     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click (Click) %s\n", CLICK_VERSION);
      printf("Copyright (C) 1999-2001 Massachusetts Institute of Technology\n\
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
  String config_str;
  if (file_is_expr)
    config_str = router_file;
  else
    config_str = file_string(router_file, errh);
  if (errh->nerrors() > 0)
    exit(1);
  if (file_is_expr)
    router_file = "<expr>";
  else if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";

  // prepare lexer (for packages)
  lexer = new Lexer(errh);
  export_elements(lexer);
  
  // find config string in archive
  if (config_str.length() != 0 && config_str[0] == '!') {
    separate_ar_string(config_str, archive, errh);
    bool found_config = false;
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name == "config") {
	config_str = archive[i].data;
	found_config = true;
      }
    if (!found_config) {
      errh->error("archive has no `config' section");
      config_str = String();
    }
  }

  // save config_str
  ::configuration_string = config_str;

  // lex
  RequireLexerExtra lextra;
  int cookie = lexer->begin_parse(config_str, router_file, &lextra);
  while (lexer->ystatement())
    /* do nothing */;
  router = lexer->create_router();
  lexer->end_parse(cookie);

  if (router->nelements() == 0 && warnings)
    errh->warning("%s: configuration has no elements", router_file);

  // handle stop option by adding a QuitWatcher element
  if (stop) {
    if (stop_guess) {
      for (int i = 0; i < router->nelements(); i++) {
	Element *e = router->element(i);
	if (e->cast("InfiniteSource") || e->cast("RatedSource")
	    || e->cast("FromDump"))
	  stops.push_back(e->id());
      }
    }
    if (!stops.size())
      errh->error("`--stop' option given, but configuration has no packet sources");
    router->add_element(new QuitWatcher, "click_driver@@QuitWatcher", cp_unargvec(stops), "click");
  }

  // add new ControlSockets
  for (int i = 0; i < ports.size(); i++)
    router->add_element(new ControlSocket, "click_driver@@ControlSocket@" + String(i), "tcp, " + String(ports[i]), "click");
  for (int i = 0; i < unix_sockets.size(); i++)
    router->add_element(new ControlSocket, "click_driver@@ControlSocket@" + String(i + ports.size()), "unix, " + cp_quote(unix_sockets[i]), "click");

  // add global handlers
  Router::add_global_read_handler("version", read_global_handler, (void *)GH_VERSION);
  Router::add_global_read_handler("list", read_global_handler, (void *)GH_LIST);
  Router::add_global_read_handler("classes", read_global_handler, (void *)GH_CLASSES);
  Router::add_global_read_handler("config", read_global_handler, (void *)GH_CONFIG);
  Router::add_global_read_handler("flatconfig", read_global_handler, (void *)GH_FLATCONFIG);
  Router::add_global_read_handler("packages", read_global_handler, (void *)GH_PACKAGES);
  Router::add_global_read_handler("requirements", read_global_handler, (void *)GH_REQUIREMENTS);
  Router::add_global_write_handler("stop", stop_global_handler, 0);

  // catch control-C and SIGTERM
  signal(SIGINT, catch_signal);
  signal(SIGTERM, catch_signal);
  // ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);
  
  if (errh->nerrors() > 0 || router->initialize(errh) < 0)
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
    started = true;
    router->thread(0)->driver();
  }

  gettimeofday(&after_time, 0);
  getrusage(RUSAGE_SELF, &after);
  // report time
  if (report_time) {
    struct timeval diff;
    timersub(&after.ru_utime, &before.ru_utime, &diff);
    printf("%ld.%03ldu", (long) diff.tv_sec, (long) (diff.tv_usec+500)/1000);
    timersub(&after.ru_stime, &before.ru_stime, &diff);
    printf(" %ld.%03lds", (long) diff.tv_sec, (long) (diff.tv_usec+500)/1000);
    timersub(&after_time, &before_time, &diff);
    printf(" %ld:%02ld.%02ld", (long) diff.tv_sec/60, (long) diff.tv_sec%60,
	   (long) (diff.tv_usec+5000)/10000);
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

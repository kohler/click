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

#include "lexer.hh"
#include "router.hh"
#include "error.hh"
#include "timer.hh"
#include "clp.h"

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define QUIT_OPT		303
#define OUTPUT_OPT		304
#define HANDLER_OPT		305

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "handler", 'h', HANDLER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "quit", 'q', QUIT_OPT, 0, 0 },
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
`Click' adds any required `Align' elements to a Click router\n\
configuration. The resulting router will work on machines that don't allow\n\
unaligned accesses. Its configuration is written to the standard output.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -h, --handler ELEMENT.HANDLER Print result of an element handler to the\n\
                                standard output.\n\
  -o, --output FILE             Write flat configuration to FILE.\n\
  -q, --quit                    Quit immediately after initializing router.\n\
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
  FileLexerSource *fp;
  if (router_file && strcmp(router_file, "-") != 0) {
    FILE *f = fopen(router_file, "r");
    if (!f)
      errh->fatal("%s: %s", router_file, strerror(errno));
    fp = new FileLexerSource(router_file, f);
  } else
    fp = new FileLexerSource("<stdin>", stdin);
  
  Lexer *lex = new Lexer(errh);
  export_elements(lex);
  lex->save_element_types();
  
  lex->reset(fp);
  while (lex->ystatement())
    /* do nothing */;
  
  Router *router = lex->create_router();
  delete fp;
  lex->clear();
  
  signal(SIGINT, catch_sigint);

  if (router->initialize(errh) < 0)
    exit(1);

  int exit_value = 0;
  
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

  if (!quit_immediately) {
    while (router->driver())
      /* nada */;
  }
  
  delete router;
  delete lex;
  exit(exit_value);
}

// generate Vector template instance
#include "vector.cc"
template class Vector<Handler>;

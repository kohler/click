#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "lexer.hh"
#include "router.hh"
#include "error.hh"
#include "timer.hh"

void
catchint(int)
{
  /* call exit so -pg file is written */
  exit(0);
}

void
print_bitvector(const Bitvector &bv)
{
  fprintf(stderr, "%d : ", bv.size());
  for (int i = 0; i < bv.size(); i++) {
    fprintf(stderr, (i % 8 == 7 ? "%c " : "%c"), (bv[i] ? '1' : '0'));
  }
  fprintf(stderr, "\n");
}

extern void export_elements(Lexer *);

int
main(int argc, char **argv)
{
  String::static_initialize();
  Timer::static_initialize();
  ErrorHandler *errh = new FileErrorHandler(stderr, "ipb: ");
  ErrorHandler::static_initialize(errh);

  bool quit_immediately = false;
  if (argc >= 2 && (strcmp(argv[1], "-q") == 0 || strcmp(argv[1], "--quit") == 0)) {
    argc--, argv++;
    quit_immediately = true;
  }
  
  FileLexerSource *fp;
  if (argc < 2)
    fp = new FileLexerSource("<stdin>", stdin);
  else
    fp = new FileLexerSource(argv[1], 0);
  
  Lexer *lex = new Lexer(errh);
  export_elements(lex);
  lex->element_types_permanent();
  
  lex->reset(fp);
  while (lex->ystatement())
    /* do nothing */;
  
  Router *router = lex->create_router();
  delete fp;
  lex->clear();
  
  if (argc > 2) {
    for (int i = 0; i < 4; i++) {
      if (router->initialize(errh) >= 0)
	router->print_structure(errh);
      fp = new FileLexerSource(argv[1], 0);
      lex->reset(fp);
      delete router;
      while (lex->ystatement())
	/* do nothing */;
      router = lex->create_router();
      lex->clear();
    }
  }
  
  signal(SIGINT, catchint);
  
  if (router->initialize(errh) >= 0) {
    //router->print_structure(errh);
    errh->message(router->flat_configuration_string());
    if (!quit_immediately)
      while (router->driver())
	/* nada */;
    delete router;
    delete lex;
    exit(0);
  } else
    exit(1);
}

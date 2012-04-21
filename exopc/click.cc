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

extern void click_export_elements(Lexer *);

int
main(int argc, char **argv)
{
  ErrorHandler *errh = new FileErrorHandler(stderr, "ipb: ");
  ErrorHandler::static_initialize(errh);

  FileLexerSource *fp;
  if (argc < 2)
    fp = new FileLexerSource("<stdin>", stdin);
  else
    fp = new FileLexerSource(argv[1], 0);

  Lexer *lex = new Lexer(errh);
  click_export_elements(lex);
  lex->element_types_permanent();

  lex->reset(fp);
  while (!lex->ydone())
      lex->ystep();

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
      while (!lex->ydone())
	  lex->ystep();
      router = lex->create_router();
      lex->clear();
    }
  }

#if 0
  signal(SIGINT, catchint);
#endif

  if (router->initialize(errh) >= 0) {
    router->print_structure(errh);
    errh->message(router->flat_configuration_string());
    while (router->driver())
      /* nada */;
  } else
  {
    delete router;
    exit(1);
  }
}

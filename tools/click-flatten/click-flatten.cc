#include <click/config.h>
#include <click/pathvars.h>

#include <click/error.hh>
#include <click/driver.hh>
#include <click/confparse.hh>
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include <click/clp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define FLATTEN_OPT		306
#define CLASSES_OPT		307
#define ELEMENTS_OPT		308
#define DECLARATIONS_OPT	309

static Clp_Option options[] = {
  { "classes", 'c', CLASSES_OPT, 0, 0 },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "decls", 'd', DECLARATIONS_OPT, 0, 0 },
  { "declarations", 'd', DECLARATIONS_OPT, 0, 0 },
  { "elements", 'n', ELEMENTS_OPT, 0, 0 },
  { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "names", 'n', ELEMENTS_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
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
`Click-flatten' reads a Click configuration, flattens it (removing any\n\
compound elements), and writes an equivalent configuration to the standard\n\
output.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -c, --classes             Output list of classes used by configuration.\n\
  -n, --names               Output list of element names in flat config.\n\
  -d, --declarations        Output list of declarations in flat config.\n\
  -f, --file FILE           Read router configuration from FILE.\n\
  -e, --expression EXPR     Use EXPR as router configuration.\n\
  -o, --output FILE         Write output configuration to FILE.\n\
  -C, --clickpath PATH      Use PATH for CLICKPATH.\n\
      --help                Print this message and exit.\n\
  -v, --version             Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

extern "C" {
  static int
  string_sorter(const void *va, const void *vb)
  {
    const String *a = (const String *)va, *b = (const String *)vb;
    return a->compare(*b);
  }
}

static void
output_sorted_one_per_line(Vector<String> &v, FILE *out)
{
  if (v.size())
    qsort((void *)&v[0], v.size(), sizeof(String), string_sorter);
  for (int i = 0; i < v.size(); i++)
    fprintf(out, "%s\n", v[i].cc());
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  bool file_is_expr = false;
  const char *output_file = 0;
  int action = FLATTEN_OPT;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;

     case VERSION_OPT:
      printf("click-flatten (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2001 Mazu Networks, Inc.\n\
Copyright (c) 2001 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
      break;

     case CLASSES_OPT:
     case DECLARATIONS_OPT:
     case ELEMENTS_OPT:
      action = opt;
      break;
      
     case OUTPUT_OPT:
      if (output_file) {
	errh->error("--output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

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

     case Clp_BadOption:
     bad_option:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  RouterT *router = read_router(router_file, file_is_expr, errh);
  if (router)
    router->flatten(errh);
  if (!router || errh->nerrors() > 0)
    exit(1);

  FILE *out;
  if (!output_file || strcmp(output_file, "-") == 0)
    out = stdout;
  else if (!(out = fopen(output_file, "wb"))) {
    errh->error("%s: %s", output_file, strerror(errno));
    exit(1);
  }

  switch (action) {

   case FLATTEN_OPT:
    write_router_file(router, out, errh);
    break;

   case CLASSES_OPT: {
     HashMap<String, int> m(-1);
     router->collect_primitive_classes(m);
     Vector<String> classes;
     for (HashMap<String, int>::iterator iter = m.begin(); iter; iter++)
       classes.push_back(iter.key());
     output_sorted_one_per_line(classes, out);
     break;
   }

   case DECLARATIONS_OPT: {
     Vector<String> decls;
     for (RouterT::iterator x = router->first_element(); x; x++)
       decls.push_back(x->name() + " :: " + x->type_name());
     output_sorted_one_per_line(decls, out);
     break;
   }

   case ELEMENTS_OPT: {
     Vector<String> elts;
     for (RouterT::iterator x = router->first_element(); x; x++)
       elts.push_back(x->name());
     output_sorted_one_per_line(elts, out);
     break;
   }
   
  }
  
  return 0;
}

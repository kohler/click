#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include <click/clp.h>
#include <stdio.h>
#include <ctype.h>

#define HELP_OPT		300
#define ROUTER_OPT		301

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s ROUTERFILE\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-shrink' produces an userlevel build environment for a specific router\n\
configuration. The resulting Makefile will build a click executable with\n\
only libclick.a and elements used by the router file.\n\
\n\
Usage: %s ROUTERFILE\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case Clp_NotOption:
     case ROUTER_OPT:
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
  RouterT *router = read_router_file(router_file, new RouterT, errh);
  if (router)
    router->flatten(errh);
  if (!router || errh->nerrors() > 0)
    exit(1);
  
  // find and parse `elementmap'
  ElementMap full_elementmap;
  full_elementmap.parse_all_required(router, CLICK_SHAREDIR, errh);
  Vector<int> elements;
  
  for (int i=0; i<router->ntypes(); i++) {
    int j = full_elementmap.find(router->type_name(i));
    if (j >= 0) 
      elements.push_back(j); 
  }
  
  int j = full_elementmap.find("QuitWatcher"); 
  for (int z=0; z<elements.size(); z++) { 
    if (elements[z] == j) { 
      j = -1; 
      break; 
    } 
  }
  if (j >= 0) elements.push_back(j); 
  j = full_elementmap.find("ControlSocket"); 
  for (int z=0; z<elements.size(); z++) { 
    if (elements[z] == j) { 
      j = -1; 
      break; 
    } 
  }
  if (j >= 0) elements.push_back(j); 
  

  for (int i=0; i<elements.size(); i++) {
    String requirement = full_elementmap.requirements(elements[i]); 
    if (requirement) { 
      Vector<String> reqs;
      cp_spacevec(requirement, reqs); 
      for (int k=0; k<reqs.size(); k++) { 
        int j = full_elementmap.find_cxx(reqs[k]);
	for (int z=0; z<elements.size(); z++) {
	  if (elements[z] == j) {
	    j = -1;
	    break;
	  }
	}
        if (j >= 0) elements.push_back(j);
      }
    }
  }

  for (int i=0; i<elements.size(); i++) {
    String fn = full_elementmap.header_file(elements[i]);
    String src;
    if (fn.substring(-3) == ".hh") { 
      src = fn.substring(0, -3) + ".cc"; 
      fprintf(stdout, "%s\n", src.cc());
    }
  }
  return 0;

}


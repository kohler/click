/*
 * click-uncombine.cc -- separate one Click configuration from a combined
 * configuration
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
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
#include "clp.h"
#include "toolutils.hh"
#include "confparse.hh"
#include "straccum.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define OUTPUT_OPT		303
#define NAME_OPT		304

static Clp_Option options[] = {
  { "component", 'c', NAME_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "name", 'n', NAME_OPT, Clp_ArgString, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String::Initializer string_initializer;
static String runclick_prog;

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
`Click-uncombine' reads a combined Click configuration produced by\n\
click-combine, separates one of its components, and writes that component to\n\
the standard output.\n\
\n\
Usage: %s [OPTION]... [COMPONENTNAME | ROUTERFILE COMPONENTNAME]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -o, --output FILE           Write output to FILE.\n\
  -c, --component NAME        Separate component named NAME.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
remove_links(RouterT *r, ErrorHandler *errh)
{
  int link_type = r->type_index("RouterLink");
  if (link_type < 0)
    return;

  Vector<int> links;
  for (int i = 0; i < r->nelements(); i++)
    if (r->etype(i) == link_type && r->ename(i).find_left('/') < 0)
      links.push_back(i);

  for (int i = 0; i < links.size(); i++) {
    Vector<String> words;
    cp_argvec(r->econfiguration(links[i]), words);
    String link_name = r->ename(links[i]);
    if (words.size() % 3 != 0)
      errh->error("RouterLink `%s' has strange configuration", link_name.cc());
    else {
      int ninputs = r->ninputs(links[i]);
      for (int j = 0; j < words.size(); j += 3) {
	if (r->eindex(words[j]) >= 0)
	  errh->error("RouterLink `%s' element `%s' already exists", link_name.cc(), words[j].cc());
	else {
	  int newe = r->get_eindex(words[j], r->get_type_index(words[j+1]), words[j+2], String());
	  if (j < ninputs)
	    r->insert_before(newe, Hookup(links[i], j));
	  else
	    r->insert_after(newe, Hookup(links[i], j - ninputs));
	}
      }
      r->free_element(links[i]);
    }
  }
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-uncombine: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool auto_router_file = true;
  String component;

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-uncombine (Click) %s\n", VERSION);
      printf("Copyright (C) 2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
      if (router_file) {
	p_errh->error("combined router specified twice");
	goto bad_option;
      }
      auto_router_file = false;
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case NAME_OPT:
      if (component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      }
      component = clp->arg;
      break;

     case Clp_NotOption:
      // if only one argument given, it's a component name
      if (router_file && component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      } else if (component)
	router_file = component;
      component = clp->arg;
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
  RouterT *r = 0;
  r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  r->flatten(errh);
      
  // remove links
  remove_links(r, errh);
  
  // find component names
  HashMap<String, int> component_map(-1);
  int link_type = r->type_index("RouterLink");
  for (int i = 0; i < r->nelements(); i++)
    if (r->elive(i)) {
      String s = r->ename(i);
      int slash = s.find_left('/');
      if (slash >= 0)
	component_map.insert(s.substring(0, slash), 0);
      else if (r->etype(i) != link_type) {
	static int warned = 0;
	if (!warned)
	  p_errh->warning("router might not be a combination (strange element names)");
	warned++;
      }
    }

  // check if component exists
  if (!component)
    p_errh->fatal("no component specified");
  else if (component_map[component] < 0)
    p_errh->fatal("no component `%s' in r router", component.cc());

  // remove everything not part of the component
  component += "/";
  int clen = component.length();
  for (int i = 0; i < r->nelements(); i++)
    if (r->elive(i)) {
      String name = r->ename(i);
      if (name.substring(0, clen) != component)
	r->kill_element(i);
      else
	r->change_ename(i, name.substring(clen));
    }

  // exit if there have been errors
  r->flatten(errh);  
  if (errh->nerrors() != 0)
    exit(1);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  write_router_file(r, outf, errh);
  exit(0);
}

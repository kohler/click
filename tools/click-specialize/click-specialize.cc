/*
 * click-specialize.cc -- specializer for Click configurations
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
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include "cxxclass.hh"
#include "clp.h"
#include <stdio.h>
#include <ctype.h>

static String::Initializer string_initializer;
static HashMap<String, int> elementinfo_map(-1);
static Vector<String> elementinfo_click_name;
static Vector<String> elementinfo_cxx_name;
static Vector<String> elementinfo_header_fn;

static void
parse_elementmap(const String &str)
{
  int p = 0;
  int len = str.length();
  const char *s = str.data();

  while (p < len) {
    
    // read a line
    while (p < len && (s[p] == ' ' || s[p] == '\t'))
      p++;

    // skip blank lines & comments
    if (p < len && !isspace(s[p]) && s[p] != '#') {

      // read Click name
      int p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      if (p >= len || (s[p] != ' ' && s[p] != '\t'))
	continue;
      String click_name = str.substring(p1, p - p1);

      // read C++ name
      while (p < len && (s[p] == ' ' || s[p] == '\t'))
	p++;
      p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      if (p >= len || (s[p] != ' ' && s[p] != '\t'))
	continue;
      String cxx_name = str.substring(p1, p - p1);

      // read header filename
      while (p < len && (s[p] == ' ' || s[p] == '\t'))
	p++;
      p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      String header_fn = str.substring(p1, p - p1);

      // append information
      p1 = elementinfo_click_name.size();
      elementinfo_click_name.push_back(click_name);
      elementinfo_cxx_name.push_back(cxx_name);
      elementinfo_header_fn.push_back(header_fn);
      elementinfo_map.insert(click_name, p1);
    }

    // skip past end of line
    while (p < len && s[p] != '\n' && s[p] != '\r' && s[p] != '\f' && s[p] != '\v')
      p++;
    p++;
  }
}


#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define OUTPUT_OPT		304

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
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
`Click-specialize' specializes.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -o, --output FILE             Write output to FILE.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-specialize (Click) %s\n", VERSION);
      printf("Copyright (C) 1999 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
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

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
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
  // find and parse `elementmap'
  {
    String elementmap_fn =
      clickpath_find_file("elementmap", "share", CLICK_SHAREDIR);
    if (!elementmap_fn)
      errh->warning("cannot find `elementmap' in CLICKPATH or `%s'", CLICK_SHAREDIR);
    else {
      String elementmap_text = file_string(elementmap_fn, errh);
      parse_elementmap(elementmap_text);
    }
  }

  // report
  int index = elementinfo_map["ARPQuerier"];

  String filename = elementinfo_header_fn[index];
  String counter_text = file_string("/u/eddietwo/src/click/" + filename, errh);
  CxxInfo cxx_info;
  cxx_info.parse_file(counter_text);
  if (filename.substring(-2) == "hh") {
    counter_text = file_string("/u/eddietwo/src/click/" + filename.substring(0, -2) + "cc", errh);
    cxx_info.parse_file(counter_text);
  }

  CxxClass *cxxc = cxx_info.find_class(elementinfo_cxx_name[index]);
  cxxc->mark_reachable_rewritable();
  /*String pattern = compile_pattern("input(#0).pull()");
  while (cxxc->replace_expr("pull", pattern, "pull_input(#0)"))
  ;*/
  
  /*
  RouterT *router = read_router_file(router_file, errh, prepared_router());
  if (!router || errh->nerrors() > 0)
    exit(1);
    router->flatten(errh);*/
}

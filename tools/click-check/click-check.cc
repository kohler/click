/*
 * click-check.cc -- check Click configurations for obvious errors
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include "error.hh"
#include "clp.h"
#include "toolutils.hh"
#include "processingt.hh"
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
#define FILTER_OPT		304
#define KERNEL_OPT		305
#define USERLEVEL_OPT		306

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "filter", 'p', FILTER_OPT, 0, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
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
`Click-check' checks a Click router configuration for correctness and reports\n\
any error messages to standard error.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE           Read router configuration from FILE.\n\
  -o, --output FILE         If valid, write configuration to FILE.\n\
  -p, --filter              If valid, write configuration to standard output.\n\
  -k, --kernel              Check kernel driver version of configuration.\n\
  -u, --user                Check user-level driver version of configuration.\n\
      --help                Print this message and exit.\n\
  -v, --version             Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
check_once(const RouterT *r, const char *filename,
	   const Vector<int> &elementmap_indexes,
	   const ElementMap &full_elementmap, int driver,
	   bool indifferent, bool print_context, bool print_ok_message,
	   ErrorHandler *full_errh)
{
  if (!indifferent && !full_elementmap.driver_compatible(elementmap_indexes, driver)) {
    full_errh->error("%s: configuration incompatible with %s driver", filename, ElementMap::driver_name(driver));
    return;
  }
  
  const ElementMap *em = &full_elementmap;
  if (!indifferent) {
    ElementMap *new_em = new ElementMap(full_elementmap);
    new_em->limit_driver(driver);
    em = new_em;
  }
  ErrorHandler *errh = full_errh;
  if (print_context)
    errh = new ContextErrorHandler(errh, "While checking configuration for " + String(ElementMap::driver_name(driver)) + " driver:");
  int before = errh->nerrors();
  int before_warnings = errh->nwarnings();

  // get processing
  ProcessingT p(r, *em, errh);
  // ... it will report errors as required

  if (print_ok_message && errh->nerrors() == before && errh->nwarnings() == before_warnings)
    full_errh->message("%s: configuration OK in %s driver", filename, ElementMap::driver_name(driver));
  if (!indifferent)
    delete em;
  if (print_context)
    delete errh;
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-check: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool output = false;
  int check_kernel = -1;
  int check_userlevel = -1;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-check (Click) %s\n", VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
     case Clp_NotOption:
      if (router_file) {
	p_errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      output = true;
      break;

     case FILTER_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = "-";
      output = true;
      break;

     case KERNEL_OPT:
      check_kernel = (clp->negated ? 0 : 1);
      break;
      
     case USERLEVEL_OPT:
      check_userlevel = (clp->negated ? 0 : 1);
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
  RouterT *r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";
  r->flatten(errh);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find and parse `elementmap'
  ElementMap elementmap;
  elementmap.parse_all_on_path(CLICK_SHAREDIR, errh);
  /* errh->warning("cannot find `elementmap' in CLICKPATH or `%s'\n(Have you done a `make install' yet?)", CLICK_SHAREDIR); */

  // parse `elementmap' from router archive
  if (r->archive_index("elementmap") >= 0)
    elementmap.parse(r->archive("elementmap").data);

  // check configuration for driver indifference
  Vector<int> elementmap_indexes;
  elementmap.map_indexes(r, elementmap_indexes, errh);
  bool indifferent = elementmap.driver_indifferent(elementmap_indexes);
  if (indifferent) {
    if (check_kernel < 0 && check_userlevel < 0)
      // only bother to check one of them
      check_kernel = 0;
  } else {
    if (check_kernel < 0 && check_userlevel < 0) {
      check_kernel = elementmap.driver_compatible(elementmap_indexes, ElementMap::DRIVER_LINUXMODULE);
      check_userlevel = elementmap.driver_compatible(elementmap_indexes, ElementMap::DRIVER_USERLEVEL);
    }
  }

  // actually check the drivers
  if (check_kernel > 0)
    check_once(r, router_file, elementmap_indexes, elementmap,
	       ElementMap::DRIVER_LINUXMODULE, indifferent,
	       check_userlevel > 0, !output,
	       errh);
  if (check_userlevel > 0)
    check_once(r, router_file, elementmap_indexes, elementmap,
	       ElementMap::DRIVER_USERLEVEL, indifferent,
	       check_kernel > 0, !output,
	       errh);
  
  // write configuration
  if (errh->nerrors() != 0)
    exit(1);
  else if (output) {
    write_router_file(r, outf, errh);
    exit(0);
  } else
    exit(0);
}

/*
 * click-check.cc -- check Click configurations for obvious errors
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/driver.hh>
#include <click/clp.h>
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
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define OUTPUT_OPT		304
#define FILTER_OPT		305
#define QUIET_OPT		306

#define FIRST_DRIVER_OPT	1000
#define LINUXMODULE_OPT		(1000 + ElementMap::DRIVER_LINUXMODULE)
#define USERLEVEL_OPT		(1000 + ElementMap::DRIVER_USERLEVEL)
#define BSDMODULE_OPT		(1000 + ElementMap::DRIVER_BSDMODULE)

static Clp_Option options[] = {
  { "bsdmodule", 'b', BSDMODULE_OPT, 0, Clp_Negate },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "filter", 'p', FILTER_OPT, 0, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', LINUXMODULE_OPT, 0, Clp_Negate },
  { "linuxmodule", 'l', LINUXMODULE_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "quiet", 'q', QUIET_OPT, 0, Clp_Negate },
  { "userlevel", 'u', USERLEVEL_OPT, 0, Clp_Negate },
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
  -b, --bsdmodule           Check for bsdmodule driver.\n\
  -l, --linuxmodule         Check for linuxmodule driver.\n\
  -u, --userlevel           Check for userlevel driver.\n\
  -C, --clickpath PATH      Use PATH for CLICKPATH.\n\
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
  const char *driver_name = ElementMap::driver_name(driver);
  
  if (!indifferent && !full_elementmap.driver_compatible(elementmap_indexes, driver)) {
    if (!full_elementmap.provides(0, driver_name))
      full_errh->error("%s: Click compiled without support for %s driver", filename, driver_name);
    else
      full_errh->error("%s: configuration incompatible with %s driver", filename, driver_name);
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
    errh = new ContextErrorHandler(errh, "While checking configuration for " + String(driver_name) + " driver:");
  int before = errh->nerrors();
  int before_warnings = errh->nwarnings();

  // get processing
  ProcessingT p(r, *em, errh);
  // ... it will report errors as required

  if (print_ok_message && errh->nerrors() == before && errh->nwarnings() == before_warnings)
    full_errh->message("%s: configuration OK in %s driver", filename, driver_name);
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
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool output = false;
  bool quiet = false;
  int driver_indifferent_mask = ElementMap::ALL_DRIVERS;
  int driver_mask = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-check (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
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

     case QUIET_OPT:
      quiet = !clp->negated;
      break;

     case LINUXMODULE_OPT:
     case USERLEVEL_OPT:
     case BSDMODULE_OPT: {
       int dm = 1 << (opt - FIRST_DRIVER_OPT);
       driver_mask = (clp->negated ? driver_mask & ~dm : driver_mask | dm);
       driver_indifferent_mask &= ~dm;
       break;
     }
      
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
  if (r)
    r->flatten(errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // parse `elementmap's
  ElementMap elementmap;
  elementmap.parse_all_files(r, CLICK_SHAREDIR, p_errh);

  // check configuration for driver indifference
  Vector<int> elementmap_indexes;
  elementmap.map_indexes(r, elementmap_indexes, errh);
  bool indifferent = elementmap.driver_indifferent(elementmap_indexes);
  if (driver_indifferent_mask == ElementMap::ALL_DRIVERS) {
    for (int d = 0; d < ElementMap::NDRIVERS; d++)
      if (elementmap.driver_compatible(elementmap_indexes, d))
	driver_mask |= 1 << d;
  }

  // actually check the drivers
  for (int d = 0; d < ElementMap::NDRIVERS; d++)
    if (driver_mask & (1 << d))
      check_once(r, router_file, elementmap_indexes, elementmap,
		 d, indifferent, driver_mask & ~(1 << d), !output && !quiet,
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

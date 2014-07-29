/*
 * click-uninstall.cc -- uninstall Click kernel module
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
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

#include "common.hh"
#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/driver.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define VERBOSE_OPT		302

static const Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]...\n\
Try '%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
'Click-uninstall' uninstalls Click from the current Linux kernel.\n\
\n\
Usage: %s [OPTION]...\n\
\n\
Options:\n\
  -V, --verbose            Print information about uninstallation process.\n\
      --help               Print this message and exit.\n\
  -v, --version            Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
}

int
main(int argc, char **argv)
{
  click_static_initialize();
  CLICK_DEFAULT_PROVIDES;
  ErrorHandler *errh = new PrefixErrorHandler(ErrorHandler::default_handler(), "click-uninstall: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

     case VERBOSE_OPT:
      verbose = !clp->negated;
      break;

     case HELP_OPT:
      usage();
      exit(0);
      break;

     case VERSION_OPT:
      printf("click-uninstall (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000-2002 Mazu Networks, Inc.\n\
Copyright (c) 2000-2002 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case Clp_NotOption:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;

     case Clp_Done:
      goto done;

    }
  }

 done:
  return (unload_click(errh) >= 0 ? 0 : 1);
}

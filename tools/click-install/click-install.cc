/*
 * click-install.cc -- configuration installer for Click kernel module
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
#include "routert.hh"
#include "lexert.hh"
#include "error.hh"
#include "confparse.hh"
#include "clp.h"
#include "toolutils.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
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
`Click-install' installs a Click configuration into the current Linux kernel.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
read_packages(HashMap<String, int> &packages, ErrorHandler *errh)
{
  packages.clear();
  FILE *f = fopen("/proc/click/packages", "r");
  if (!f)
    errh->warning("cannot read /proc/click/packages: %s", strerror(errno));
  else {
    char buf[1024];
    while (fgets(buf, 1024, f)) {
      String p = buf;
      if (p[p.length()-1] != '\n')
	errh->warning("/proc/click/packages: line too long");
      else
	packages.insert(p.substring(0, -1), 0);
    }
    fclose(f);
  }
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-install: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-install (Click) %s\n", VERSION);
      printf("Copyright (C) 1999 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
     case Clp_NotOption:
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
  RouterT *r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  
  r->flatten(errh);

  // check for Click module; install it if not available
  {
    if (access("/proc/click", F_OK) < 0) {
      // try to install module
      String click_o =
	clickpath_find_file("click.o", "lib", CLICK_LIBDIR, errh);
      String cmdline = "/sbin/insmod " + click_o;
      (void) system(cmdline);
      if (access("/proc/click", F_OK) < 0)
	errh->fatal("cannot install Click module");
    }
  }

  // find active modules
  HashMap<String, int> active_modules(-1);
  {
    String s = file_string("/proc/modules", errh);
    int p = 0;
    while (p < s.length()) {
      int start = p;
      while (!isspace(s[p])) p++;
      active_modules.insert(s.substring(start, p - start), 0);
      p = s.find_left('\n', p) + 1;
    }
  }
  
  // find current packages
  HashMap<String, int> packages(-1);
  read_packages(packages, errh);

  // install archived objects. mark them with leading underscores.
  // may require renaming to avoid clashes in `insmod'
  {
    const Vector<ArchiveElement> &archive = r->archive();
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name.length() > 2
	  && archive[i].name.substring(-2) == ".ko") {
	
	// choose module name
	String module_name = archive[i].name.substring(0, -3);
	String insmod_name = "_" + module_name;
	while (active_modules[insmod_name] >= 0)
	  insmod_name = "_" + insmod_name;

	// install module
	String tmpnam = unique_tmpnam("x*.o", errh);
	if (!tmpnam) exit(1);
	FILE *f = fopen(tmpnam.cc(), "w");
	fwrite(archive[i].data.data(), 1, archive[i].data.length(), f);
	fclose(f);
	String cmdline = "/sbin/insmod -o " + insmod_name + " " + tmpnam;
	int retval = system(cmdline);
	if (retval != 0)
	  errh->fatal("`insmod %s' failed: %s", module_name.cc(), strerror(errno));

	// cleanup
	packages.insert(module_name, 1);
	active_modules.insert(insmod_name, 1);
      }
  }
  
  // install missing requirements
  {
    const HashMap<String, int> &requirements = r->requirement_map();
    int thunk = 0, value; String key;
    while (requirements.each(thunk, key, value))
      if (value >= 0 && packages[key] < 0) {
	String package = clickpath_find_file
	  (key + ".ko", "lib", CLICK_LIBDIR);
	if (!package)
	  package = clickpath_find_file
	    (key + ".o", "lib", CLICK_LIBDIR);
	if (!package) {
	  errh->message("cannot find required package `%s.o'", key.cc());
	  errh->fatal("in CLICKPATH or `%s'", CLICK_LIBDIR);
	}
	String cmdline = "/sbin/insmod " + package;
	int retval = system(cmdline);
	if (retval != 0)
	  errh->fatal("`insmod %s' failed: %s", package.cc(), strerror(errno));
	active_modules.insert(package, 1);
      }
  }
  
  // write flattened configuration to /proc/click/config
  FILE *f = fopen("/proc/click/config", "w");
  if (!f)
    errh->fatal("cannot install configuration: %s", strerror(errno));
  // XXX include packages?
  String config = r->configuration_string();
  fwrite(config.data(), 1, config.length(), f);
  fclose(f);

  // report errors
  {
    char buf[1024];
    FILE *f = fopen("/proc/click/errors", "r");
    if (!f)
      errh->warning("cannot read /proc/click/errors: %s", strerror(errno));
    else {
      while (!feof(f)) {
	size_t s = fread(buf, 1, 1024, f);
	fwrite(buf, 1, s, stderr);
      }
      fclose(f);
    }
  }

  // remove unused packages
  {
    int thunk = 0, value; String key;
    String to_remove;
    // go over all modules; figure out which ones are Click packages
    // by checking `packages' array; mark old Click packages for removal
    while (active_modules.each(thunk, key, value))
      // only remove packages that weren't used in this configuration.
      // packages used in this configuration have value > 0
      if (value == 0) {
	if (packages[key] >= 0)
	  to_remove += " " + key;
	else {
	  // check for removing an old archive package;
	  // they are identified by a leading underscore.
	  int p;
	  for (p = 0; p < key.length() && key[p] == '_'; p++)
	    /* nada */;
	  String s = key.substring(p);
	  if (s && packages[s] >= 0)
	    to_remove += " " + key;
	}
      }
    // actually call `rmmod'
    if (to_remove) {
      String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
      (void) system(cmdline);
    }
  }
  
  return 0;
}

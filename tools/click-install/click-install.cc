/*
 * click-install.cc -- configuration installer for Click kernel module
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include <click/pathvars.h>

#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/clp.h>
#include <click/package.hh>
#include "toolutils.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define UNINSTALL_OPT		304
#define HOTSWAP_OPT		305
#define MAP_OPT			306
#define VERBOSE_OPT		307
#define THREADS_OPT		308
#define PRIVATE_OPT		309
#define PRIORITY_OPT		310

static Clp_Option options[] = {
  { "cabalistic", 0, PRIVATE_OPT, 0, Clp_Negate },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "hot-swap", 'h', HOTSWAP_OPT, 0, Clp_Negate },
  { "map", 'm', MAP_OPT, 0, 0 },
  { "priority", 'n', PRIORITY_OPT, Clp_ArgInt, 0 },
  { "private", 'p', PRIVATE_OPT, 0, Clp_Negate },
  { "threads", 't', THREADS_OPT, Clp_ArgUnsigned, 0 },
  { "uninstall", 'u', UNINSTALL_OPT, 0, Clp_Negate },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, Clp_Negate },
};

static const char *program_name;
static bool verbose;
static bool output_map;

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
  -f, --file FILE          Read router configuration from FILE.\n\
  -h, --hot-swap           Hot-swap install new configuration.\n\
  -u, --uninstall          Uninstall Click from kernel, then reinstall.\n\
  -m, --map                Print load map to the standard output.\n\
  -n, --priority N         Set kernel thread priority to N (lower is better).\n\
  -p, --private            Make /proc/click readable only by root.\n\
  -t, --threads N          Use N threads (multithreaded Click only).\n\
  -V, --verbose            Print information about files installed.\n\
  -C, --clickpath PATH     Use PATH for CLICKPATH.\n\
      --help               Print this message and exit.\n\
  -v, --version            Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static bool
prepare_compile_tmpdir(RouterT *r, String &tmpdir, String &compile_prog,
		       ErrorHandler *errh)
{
  ContextErrorHandler cerrh(errh, "While preparing to compile packages:");
  
  // change to temporary directory
  tmpdir = click_mktmpdir(&cerrh);
  if (!tmpdir)
    return false;
  if (chdir(tmpdir.cc()) < 0)
    cerrh.fatal("cannot chdir to %s: %s", tmpdir.cc(), strerror(errno));

  // find compile program
  compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, &cerrh);
  if (!compile_prog)
    return false;

  // look for .hh files
  const Vector<ArchiveElement> &archive = r->archive();  
  for (int i = 0; i < archive.size(); i++)
    if (archive[i].name.substring(-3) == ".hh") {
      String filename = archive[i].name;
      FILE *f = fopen(filename, "w");
      if (!f)
	cerrh.warning("%s: %s", filename.cc(), strerror(errno));
      else {
	fwrite(archive[i].data.data(), 1, archive[i].data.length(), f);
	fclose(f);
      }
    }

  return true;
}

static void
compile_archive_packages(RouterT *r, ErrorHandler *errh)
{
  Vector<String> requirements = r->requirements();

  String tmpdir;
  String click_compile_prog;
  
  // go over requirements
  for (int i = 0; i < requirements.size(); i++) {
    const String &req = requirements[i];

    // skip if already have .ko
    if (r->archive_index(req + ".ko") >= 0)
      continue;

    // look for .k.cc or .cc
    int source_ae = r->archive_index(req + ".k.cc");
    if (source_ae < 0)
      source_ae = r->archive_index(req + ".cc");
    if (source_ae < 0)
      continue;

    // found source file, so compile it
    ArchiveElement ae = r->archive(source_ae);
    if (!click_compile_prog)
      if (!prepare_compile_tmpdir(r, tmpdir, click_compile_prog, errh))
	exit(1);

    errh->message("Compiling package %s from config archive", ae.name.cc());
    ContextErrorHandler cerrh
      (errh, "While compiling package `" + req + ".ko':");

    // write .cc file
    String filename = req + ".cc";
    String source_text = ae.data;
    FILE *f = fopen(filename, "w");
    if (!f)
      cerrh.fatal("%s: %s", filename.cc(), strerror(errno));
    fwrite(source_text.data(), 1, source_text.length(), f);
    fclose(f);
    
    // run click-compile
    String compile_command = click_compile_prog + " --target=kernel --package=" + req + ".ko " + filename;
    int compile_retval = system(compile_command.cc());
    if (compile_retval == 127)
      cerrh.fatal("could not run `%s'", compile_command.cc());
    else if (compile_retval < 0)
      cerrh.fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
    else if (compile_retval != 0)
      cerrh.fatal("`%s' failed", compile_command.cc());
    
    // grab object file and add to archive
    ArchiveElement obj_ae = init_archive_element(req + ".ko", 0600);
    obj_ae.data = file_string(req + ".ko", &cerrh);
    r->add_archive(obj_ae);
  }
}

static void
install_required_packages(RouterT *r, HashMap<String, int> &packages,
			  HashMap<String, int> &active_modules,
			  ErrorHandler *errh)
{
  // check for uncompiled archive packages and try to compile them
  compile_archive_packages(r, errh);
  
  Vector<String> requirements = r->requirements();

  // go over requirements
  for (int i = 0; i < requirements.size(); i++) {
    String req = requirements[i];

    // look for object in archive
    int obj_aei = r->archive_index(req + ".ko");
    if (obj_aei >= 0) {
      // install archived objects. mark them with leading underscores.
      // may require renaming to avoid clashes in `insmod'
      
      // choose module name
      String insmod_name = "_" + req;
      while (active_modules[insmod_name] >= 0)
	insmod_name = "_" + insmod_name;

      if (verbose)
	errh->message("Installing package %s (%s.ko from config archive)", insmod_name.cc(), req.cc());
	
      // install module
      const ArchiveElement &ae = r->archive(obj_aei);
      String tmpnam = unique_tmpnam("x*.o", errh);
      if (!tmpnam) exit(1);
      FILE *f = fopen(tmpnam.cc(), "w");
      fwrite(ae.data.data(), 1, ae.data.length(), f);
      fclose(f);
      String cmdline = "/sbin/insmod ";
      if (output_map)
	cmdline += "-m ";
      cmdline += "-o " + insmod_name + " " + tmpnam;
      int retval = system(cmdline);
      if (retval != 0)
	errh->fatal("`insmod %s' failed", req.cc());

      // cleanup
      packages.insert(req, 1);
      active_modules.insert(insmod_name, 1);
      
    } else if (packages[req] < 0) {
      // install required package from CLICKPATH
      String fn = clickpath_find_file(req + ".ko", "lib", CLICK_LIBDIR);
      if (!fn)
	fn = clickpath_find_file(req + ".o", "lib", CLICK_LIBDIR);
      if (!fn)
	errh->fatal("cannot find required package `%s.ko'\nin CLICKPATH or `%s'", req.cc(), CLICK_LIBDIR);

      // install module
      if (verbose)
	errh->message("Installing package %s (%s)", req.cc(), fn.cc());
      String cmdline = "/sbin/insmod ";
      if (output_map)
	cmdline += "-m ";
      cmdline += fn;
      int retval = system(cmdline);
      if (retval != 0)
	errh->fatal("`insmod %s' failed: %s", fn.cc(), strerror(errno));
      active_modules.insert(req, 1);
    }
  }
}

static bool
read_package_file(String filename, StringMap &packages, ErrorHandler *errh)
{
  if (!errh && access(filename.cc(), F_OK) < 0)
    return false;
  String text = file_string(filename, errh);
  const char *s = text.data();
  int pos = 0;
  int len = text.length();
  while (pos < len) {
    int start = pos;
    while (pos < len && !isspace(s[pos]))
      pos++;
    packages.insert(text.substring(start, pos - start), 0);
    pos = text.find_left('\n', pos) + 1;
  }
  return (bool)text;
}

static String
packages_to_remove(const StringMap &active_modules, const StringMap &packages)
{
  // remove extra packages
  String to_remove;
  // go over all modules; figure out which ones are Click packages
  // by checking `packages' array; mark old Click packages for removal
  for (StringMap::Iterator iter = active_modules.first(); iter; iter++)
    // only remove packages that weren't used in this configuration.
    // packages used in this configuration have value > 0
    if (iter.value() == 0) {
      String key = iter.key();
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
	else if (key.length() > 3 && key.substring(key.length() - 3) == ".ko") {
	  // check for .ko packages
	  s = key.substring(0, key.length() - 3);
	  if (s && packages[s] >= 0)
	    to_remove += " " + key;
	}
      }
    }
  return to_remove;
}

static void
kill_current_configuration(ErrorHandler *errh)
{
  if (verbose)
    errh->message("Writing blank configuration to /proc/click/config");
  FILE *f = fopen("/proc/click/config", "w");
  if (!f)
    errh->fatal("cannot uninstall configuration: %s", strerror(errno));
  fputs("// nothing\n", f);
  fclose(f);

  // wait for thread to die
  if (verbose)
    errh->message("Waiting for Click threads to die");
  for (int wait = 0; wait < 6; wait++) {
    String s = file_string("/proc/click/threads");
    if (!s || s == "0\n")
      return;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(0, 0, 0, 0, &tv);
  }
  errh->error("failed to kill current Click configuration");
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *nop_errh = ErrorHandler::default_handler();
  ErrorHandler *errh = new PrefixErrorHandler(nop_errh, "click-install: ");
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  int threads = 1;
  bool uninstall = false;
  bool hotswap = false;
  bool accessible = true;
  int priority = -100;
  output_map = false;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-install (Click) %s\n", CLICK_VERSION);
      printf("Click packages in %s, binaries in %s\n", CLICK_LIBDIR, CLICK_BINDIR);
      printf("Copyright (c) 1999-2000 Massachusetts Institute of Technology\n\
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
	errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;
     
     case THREADS_OPT:
      threads = clp->val.u;
      if (threads < 1) {
        errh->error("must have at least one thread");
	goto bad_option;
      }
      break;

     case PRIVATE_OPT:
      accessible = clp->negated;
      break;

     case PRIORITY_OPT:
      priority = clp->val.i;
      break;

     case UNINSTALL_OPT:
      uninstall = !clp->negated;
      break;

     case HOTSWAP_OPT:
      hotswap = !clp->negated;
      break;

     case MAP_OPT:
      output_map = !clp->negated;
      break;

     case VERBOSE_OPT:
      verbose = !clp->negated;
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
  // check options
  if (hotswap && uninstall)
    errh->warning("`--hotswap' and `--uninstall' are mutually exclusive");
  
  RouterT *r = read_router_file(router_file, nop_errh);
  if (r)
    r->flatten(nop_errh);
  if (!r || errh->nerrors() > 0)
    exit(1);

  // uninstall Click if requested
  if (uninstall && access("/proc/click/version", F_OK) >= 0) {
    // install blank configuration
    kill_current_configuration(errh);
    // find current packages
    HashMap<String, int> active_modules(-1);
    HashMap<String, int> packages(-1);
    read_package_file("/proc/modules", active_modules, errh);
    read_package_file("/proc/click/packages", packages, errh);
    // remove packages
    String to_remove = packages_to_remove(active_modules, packages);
    if (to_remove) {
      if (verbose)
	errh->message("Removing packages:%s", to_remove.cc());
      String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
      (void) system(cmdline);
    }
    if (verbose)
      errh->message("Removing Click module");
    (void) system("/sbin/rmmod click");

    // see if we successfully removed it
    if (access("/proc/click/version", F_OK) >= 0)
      errh->warning("could not uninstall Click module");
  }
  
  // check for Click module; install it if not available
  if (access("/proc/click/version", F_OK) < 0) {
    String click_o =
      clickpath_find_file("click.o", "lib", CLICK_LIBDIR, errh);
    if (verbose)
      errh->message("Installing Click module (%s)", click_o.cc());
    String cmdline = "/sbin/insmod ";
    if (output_map)
      cmdline += "-m ";
    cmdline += click_o;
    if (threads > 1) {
      cmdline += " threads=";
      cmdline += String(threads);
    }
    if (!accessible)
      cmdline += " accessible=0";
    (void) system(cmdline);
    if (access("/proc/click/version", F_OK) < 0)
      errh->fatal("cannot install Click module");
  }

  // find current packages
  HashMap<String, int> active_modules(-1);
  HashMap<String, int> packages(-1);
  read_package_file("/proc/modules", active_modules, errh);
  read_package_file("/proc/click/packages", packages, errh);

  // install required packages
  install_required_packages(r, packages, active_modules, errh);

  // set priority
  if (priority > -100) {
    FILE *f = fopen("/proc/click/priority", "w");
    if (!f)
      errh->fatal("cannot open /proc/click/priority: %s", strerror(errno));
    fprintf(f, "%d\n", priority);
    fclose(f);
  }

  // write flattened configuration to /proc/click/config
  const char *config_place = (hotswap ? "/proc/click/hotconfig" : "/proc/click/config");
  if (verbose)
    errh->message("Writing configuration to %s", config_place);
  FILE *f = fopen(config_place, "w");
  if (!f)
    errh->fatal("cannot install configuration: %s", strerror(errno));
  // XXX include packages?
  String config = r->configuration_string();
  fwrite(config.data(), 1, config.length(), f);
  fclose(f);

  // report errors
  {
    char buf[1024];
    int fd = open("/proc/click/errors", O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      errh->warning("/proc/click/errors: %s", strerror(errno));
    else {
      off_t pos = 0;
      struct stat s;
      while (1) {
	if (fstat(fd, &s) < 0) { // find length of errors file
	  errh->error("/proc/click/errors: %s", strerror(errno));
	  break;
	}
	if (pos >= s.st_size)
	  break;
	size_t want = s.st_size - pos;
	if (want > 1024 || want <= 0)
	  want = 1024;
	ssize_t got = read(fd, buf, want);
	if (got >= 0) {
	  fwrite(buf, 1, got, stderr);
	  pos += got;
	} else if (errno != EINTR && errno != EAGAIN) {
	  errh->error("/proc/click/errors: %s", strerror(errno));
	  break;
	}
      }
      close(fd);
    }
  }

  // remove unused packages
  {
    String to_remove = packages_to_remove(active_modules, packages);
    if (to_remove) {
      if (verbose)
	errh->message("Removing old packages:%s", to_remove.cc());
      String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
      (void) system(cmdline);
    }
  }
  
  return 0;
}

/*
 * click-install.cc -- configuration installer for Click kernel module
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define UNINSTALL_OPT		303
#define HOTSWAP_OPT		304
#define MAP_OPT			305
#define VERBOSE_OPT		306

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "hot-swap", 'h', HOTSWAP_OPT, 0, Clp_Negate },
  { "map", 'm', MAP_OPT, 0, 0 },
  { "uninstall", 'u', UNINSTALL_OPT, 0, Clp_Negate },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, Clp_Negate },
};

static const char *program_name;
static bool verbose;

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
  -h, --hot-swap                Hot-swap install new configuration.\n\
  -u, --uninstall               Uninstall Click from kernel, then reinstall.\n\
  -m, --map                     Print load map to the standard output.\n\
  -V, --verbose                 Print information about files installed.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
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
  const StringMap &requirements = r->requirement_map();
  const Vector<ArchiveElement> &archive = r->archive();
  StringMap have_requirements(0);

  // analyze archive
  for (int i = 0; i < archive.size(); i++) {
    const ArchiveElement &ae = archive[i];
    if (ae.name.substring(-5) == ".k.cc") {
      int &have = have_requirements.find_force(ae.name.substring(0, -5));
      have |= 1;
    } if (ae.name.substring(-3) == ".cc" && (ae.name.length() < 5 || ae.name[ae.name.length()-5] != '.')) {
      int &have = have_requirements.find_force(ae.name.substring(0, -3));
      have |= 1;
    } else if (ae.name.substring(-3) == ".ko") {
      int &have = have_requirements.find_force(ae.name.substring(0, -3));
      have |= 2;
    }
  }
  
  String tmpdir;
  String click_compile_prog;
  
  // check requirements
  for (StringMap::Iterator iter = requirements.first(); iter; iter++) {
    String package = iter.key();
    int value = iter.value();
    if (value > 0 && have_requirements[package] == 1) {
      // have source, but not package; compile it
      
      if (!click_compile_prog)
	if (!prepare_compile_tmpdir(r, tmpdir, click_compile_prog, errh))
	  exit(1);

      errh->message("Compiling package %s.cc from config archive", package.cc());
      ContextErrorHandler cerrh
	(errh, "While compiling package `" + package + ".ko':");

      // write .cc file
      String filename = package + ".k.cc";
      if (r->archive_index(filename) < 0)
	filename = package + ".cc";
      assert(r->archive_index(filename) >= 0);
      String source_text = r->archive(filename).data;
      FILE *f = fopen(filename, "w");
      if (!f)
	cerrh.fatal("%s: %s", filename.cc(), strerror(errno));
      fwrite(source_text.data(), 1, source_text.length(), f);
      fclose(f);
      // grab compiler options
      String compiler_options;
      if (source_text.substring(0, 17) == "// click-compile:") {
	const char *s = source_text.data();
	int pos = 17;
	int len = source_text.length();
	while (pos < len && s[pos] != '\n' && s[pos] != '\r')
	  pos++;
	// XXX check user input for shell metas?
	compiler_options = source_text.substring(17, pos - 17) + " ";
      }
      
      // run click-compile
      String compile_command = click_compile_prog + " --target=kernel --package=" + package + ".ko " + compiler_options + filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	cerrh.fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	cerrh.fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	cerrh.fatal("`%s' failed", compile_command.cc());

      // grab object file and add to archive
      ArchiveElement ae = init_archive_element(package + ".ko", 0600);
      ae.data = file_string(package + ".ko", &cerrh);
      r->add_archive(ae);
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
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-install: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  bool uninstall = false;
  bool hotswap = false;
  bool output_map = false;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-install (Click) %s\n", VERSION);
      printf("Click packages in %s, binaries in %s\n", CLICK_LIBDIR, CLICK_BINDIR);
      printf("Copyright (c) 1999-2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
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
  
  RouterT *r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  r->flatten(errh);

  // uninstall Click if requested
  if (uninstall && access("/proc/click", F_OK) >= 0) {
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
    if (access("/proc/click", F_OK) >= 0)
      errh->warning("could not uninstall Click module");
  }
  
  // check for Click module; install it if not available
  if (access("/proc/click", F_OK) < 0) {
    String click_o =
      clickpath_find_file("click.o", "lib", CLICK_LIBDIR, errh);
    if (verbose)
      errh->message("Installing Click module %s", click_o.cc());
    String cmdline = "/sbin/insmod ";
    if (output_map)
      cmdline += "-m ";
    cmdline += click_o;
    (void) system(cmdline);
    if (access("/proc/click", F_OK) < 0)
      errh->fatal("cannot install Click module");
  }

  // find current packages
  HashMap<String, int> active_modules(-1);
  HashMap<String, int> packages(-1);
  read_package_file("/proc/modules", active_modules, errh);
  read_package_file("/proc/click/packages", packages, errh);

  // check for uncompiled archive packages and try to compile them
  compile_archive_packages(r, errh);
  
  // install archived objects. mark them with leading underscores.
  // may require renaming to avoid clashes in `insmod'
  {
    const Vector<ArchiveElement> &archive = r->archive();
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name.length() > 2
	  && archive[i].name.substring(-3) == ".ko") {
	
	// choose module name
	String module_name = archive[i].name.substring(0, -3);
	String insmod_name = "_" + module_name;
	while (active_modules[insmod_name] >= 0)
	  insmod_name = "_" + insmod_name;

	if (verbose)
	  errh->message("Installing package %s (%s.ko from config archive)", insmod_name.cc(), module_name.cc());
	
	// install module
	String tmpnam = unique_tmpnam("x*.o", errh);
	if (!tmpnam) exit(1);
	FILE *f = fopen(tmpnam.cc(), "w");
	fwrite(archive[i].data.data(), 1, archive[i].data.length(), f);
	fclose(f);
	String cmdline = "/sbin/insmod -o " + insmod_name + " " + tmpnam;
	int retval = system(cmdline);
	if (retval != 0)
	  errh->fatal("`insmod %s' failed", module_name.cc());

	// cleanup
	packages.insert(module_name, 1);
	active_modules.insert(insmod_name, 1);
      }
  }
  
  // install missing requirements
  {
    const StringMap &requirements = r->requirement_map();
    for (StringMap::Iterator iter = requirements.first(); iter; iter++) {
      String key = iter.key();
      int value = iter.value();
      if (value > 0 && packages[key] < 0) {
	String package = clickpath_find_file
	  (key + ".ko", "lib", CLICK_LIBDIR);
	if (!package)
	  package = clickpath_find_file
	    (key + ".o", "lib", CLICK_LIBDIR);
	if (!package) {
	  errh->message("cannot find required package `%s.o'", key.cc());
	  errh->fatal("in CLICKPATH or `%s'", CLICK_LIBDIR);
	}
	if (verbose)
	  errh->message("Installing package %s (%s)", key.cc(), package.cc());
	String cmdline = "/sbin/insmod " + package;
	int retval = system(cmdline);
	if (retval != 0)
	  errh->fatal("`insmod %s' failed: %s", package.cc(), strerror(errno));
	active_modules.insert(package, 1);
      }
    }
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

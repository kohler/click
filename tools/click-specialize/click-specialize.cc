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
#include "archive.hh"
#include "specializer.hh"
#include "clp.h"
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define OUTPUT_OPT		304
#define KERNEL_OPT		305
#define USERLEVEL_OPT		306
#define SOURCE_OPT		307

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "source", 's', SOURCE_OPT, 0, Clp_Negate },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;


String
click_to_cxx_name(const String &click)
{
  StringAccum sa;
  const char *s = click.data();
  const char *end_s = s + click.length();
  for (; s < end_s; s++)
    if (*s == '_')
      sa << "_u";
    else if (*s == '@')
      sa << "_a";
    else if (*s == '/')
      sa << "_s";
    else
      sa << *s;
  return sa.take_string();
}

String
specialized_click_name(RouterT *router, int i)
{
  return router->etype_name(i) + "@@" + router->ename(i);
}

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
`Click-specialize' transforms a router configuration by generating new code\n\
for its elements. This new code removes virtual function calls from the packet\n\
control path. The resulting configuration has both Click-language files and\n\
object files.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -o, --output FILE           Write output to FILE.\n\
  -k, --kernel                Create Linux kernel module code (on by default).\n\
  -u, --user                  Create user-level code (on by default).\n\
  -s, --source                Write source code only.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
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
  int source_only = 0;
  int compile_kernel = -1;
  int compile_user = -1;
  
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
      
     case SOURCE_OPT:
      source_only = !clp->negated;
      break;
      
     case KERNEL_OPT:
      compile_kernel = !clp->negated;
      break;
      
     case USERLEVEL_OPT:
      compile_user = !clp->negated;
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
  if (compile_kernel < 0 && compile_user < 0) {
#ifdef HAVE_LINUXMODULE_TARGET
    compile_kernel = compile_user = 1;
#else
    compile_user = 1;
#endif
  }

  // read router
  RouterT *router = read_router_file(router_file, errh);
  if (!router || errh->nerrors() > 0)
    exit(1);
  router->flatten(errh);
  Specializer specializer(router);
  
  // find and parse `elementmap'
  {
    String elementmap_fn =
      clickpath_find_file("elementmap", "share", CLICK_SHAREDIR);
    if (!elementmap_fn)
      errh->warning("cannot find `elementmap' in CLICKPATH or `%s'", CLICK_SHAREDIR);
    else {
      String elementmap_text = file_string(elementmap_fn, errh);
      specializer.parse_elementmap(elementmap_text);
    }
  }

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find Click binaries
  String click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

  // actually specialize
  specializer.specialize(errh);

  // output
  StringAccum out;
  out << "#ifdef HAVE_CONFIG_H\n\
# include <config.h>\n\
#endif\n\
#include \"clickpackage.hh\"\n";
  specializer.output(out);
  
  // find name of package
  String package_name = "specialize";
  int uniqueifier = 1;
  while (1) {
    if (router->archive(package_name) < 0)
      break;
    uniqueifier++;
    package_name = "specialize" + String(uniqueifier);
  }
  router->add_requirement(package_name);

  specializer.output_package(package_name, out);
  out << '\0';

  // output source code if required
  if (source_only) {
    fputs(out.data(), outf);
    fclose(outf);
    return 0;
  }
  
  // create temporary directory
  String tmpdir = click_mktmpdir(errh);
  if (!tmpdir) exit(1);
  if (chdir(tmpdir.cc()) < 0)
    errh->fatal("cannot chdir to %s: %s", tmpdir.cc(), strerror(errno));
  
  // write C++ file
  String cxx_filename = package_name + "x.cc";
  FILE *f = fopen(cxx_filename, "w");
  if (!f)
    errh->fatal("%s: %s", cxx_filename.cc(), strerror(errno));
  fputs(out.data(), f);
  fclose(f);

  // compile kernel module
  if (compile_kernel > 0) {
    String compile_command = click_compile_prog + " --target=kernel --package=" + package_name + ".ko -fno-access-control " + cxx_filename;
    int compile_retval = system(compile_command.cc());
    if (compile_retval == 127)
      errh->fatal("could not run `%s'", compile_command.cc());
    else if (compile_retval < 0)
      errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
    else if (compile_retval != 0)
      errh->fatal("`%s' failed", compile_command.cc());
  }

  // compile userlevel
  if (compile_user > 0) {
    String compile_command = click_compile_prog + " --target=user --package=" + package_name + ".uo -w -fno-access-control " + cxx_filename;
    int compile_retval = system(compile_command.cc());
    if (compile_retval == 127)
      errh->fatal("could not run `%s'", compile_command.cc());
    else if (compile_retval < 0)
      errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
    else if (compile_retval != 0)
      errh->fatal("`%s' failed", compile_command.cc());
  }

  // retype elements
  specializer.fix_elements();
  
  // read .cc and .?o files, add them to archive
  {
    ArchiveElement ae;
    ae.name = package_name + ".cc";
    ae.date = time(0);
    ae.uid = geteuid();
    ae.gid = getegid();
    ae.mode = 0600;
    ae.data = file_string(cxx_filename, errh);
    router->add_archive(ae);

    if (compile_kernel > 0) {
      ae.name = package_name + ".ko";
      ae.data = file_string(package_name + ".ko", errh);
      router->add_archive(ae);
    }
    
    if (compile_user > 0) {
      ae.name = package_name + ".uo";
      ae.data = file_string(package_name + ".uo", errh);
      router->add_archive(ae);
    }
  }
  
  // write configuration
  write_router_file(router, outf, errh);
  return 0;
}

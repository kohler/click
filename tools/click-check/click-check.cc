/*
 * click-fastclassifier.cc -- specialize Click classifiers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
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
#include "straccum.hh"
#include "clp.h"
#include "toolutils.hh"
#include "subvector.hh"
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
#define KERNEL_OPT		304
#define USERLEVEL_OPT		305
#define SOURCE_OPT		306
#define CONFIG_OPT		307
#define REVERSE_OPT		308

static Clp_Option options[] = {
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "reverse", 'r', REVERSE_OPT, 0, Clp_Negate },
  { "source", 's', SOURCE_OPT, 0, Clp_Negate },
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
`Click-fastclassifier' transforms a router configuration by replacing generic\n\
Classifier elements with specific generated code. The resulting configuration\n\
has both Click-language files and object files.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -o, --output FILE           Write output to FILE.\n\
  -k, --kernel                Compile into Linux kernel binary package.\n\
  -u, --user                  Compile into user-level binary package.\n\
  -s, --source                Write source code only.\n\
  -c, --config                Write new configuration only.\n\
  -r, --reverse               Reverse transformation.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


// Classifier related stuff

static String
get_string_from_process(String cmdline, const String &input,
			ErrorHandler *errh)
{
  FILE *f = tmpfile();
  if (!f)
    errh->fatal("cannot create temporary file: %s", strerror(errno));
  fwrite(input.data(), 1, input.length(), f);
  fflush(f);
  rewind(f);
  
  String new_cmdline = cmdline + " 0<&" + String(fileno(f));
  FILE *p = popen(new_cmdline.cc(), "r");
  if (!p)
    errh->fatal("`%s': %s", cmdline.cc(), strerror(errno));

  StringAccum sa;
  while (!feof(p) && sa.length() < 10000) {
    int x = fread(sa.reserve(2048), 1, 2048, p);
    if (x > 0) sa.forward(x);
  }
  if (!feof(p))
    errh->warning("`%s' output too long, truncated", cmdline.cc());

  fclose(f);
  pclose(p);
  return sa.take_string();
}

static void
print_vector(const char *x, const Vector<int> &v)
{
  fprintf(stderr, "%s [", x);
  for (int i = 0; i < v.size(); i++)
    fprintf(stderr, (i ? " %d" : "%d"), v[i]);
  fprintf(stderr, "]\n");
}

static void
print_vector(const char *x, const Subvector<int> &v)
{
  fprintf(stderr, "%s [", x);
  for (int i = 0; i < v.size(); i++)
    fprintf(stderr, (i ? " %d" : "%d"), v[i]);
  fprintf(stderr, "]\n");
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-fastclassifier: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  int compile_kernel = 0;
  int compile_user = 0;
  bool source_only = false;
  bool config_only = false;
  bool reverse = false;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-fastclassifier (Click) %s\n", VERSION);
      printf("Copyright (C) 1999-2000 Massachusetts Institute of Technology\n\
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

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case REVERSE_OPT:
      reverse = !clp->negated;
      break;
      
     case SOURCE_OPT:
      source_only = !clp->negated;
      break;
      
     case CONFIG_OPT:
      config_only = !clp->negated;
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
  RouterT *r = read_router_file(router_file, errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  r->flatten(errh);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  {
    Vector<int> v;
    for (int i = 0; i < 10; i++) {
      v.push_back(i);
      print_vector("V", v);
    }
    print_vector("V", v);
    {
      Subvector<int> q(v, 4, 5);
      print_vector("V", v);
      print_vector("Q", q);
      q[3] = 12;
      print_vector("V", v);
      print_vector("Q", q);
    }
    print_vector("V", v);
  }
  
  // write configuration
  write_router_file(r, outf, errh);
  exit(0);
}

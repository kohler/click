/*
 * click-fastclassifier.cc -- specialize Click classifiers
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
#include "straccum.hh"
#include "clp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/wait.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 'h', HELP_OPT, 0, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String::Initializer string_initializer;
static String click_binary;

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
`Click-fastclassifier' sucks ass.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -h, --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static String
path_find_file_2(const String &filename, String path, String default_path,
		 String subdir)
{
  if (subdir.back() != '/') subdir += "/";
  
  while (1) {
    int colon = path.find_left(':');
    String dir = (colon < 0 ? path : path.substring(0, colon));
    
    if (!dir && default_path) {
      // look in default path
      String s = path_find_file_2(filename, default_path, String(), 0);
      if (s) return s;
      default_path = String();	// don't search default path twice
      
    } else if (dir) {
      if (dir.back() != '/') dir += "/";
      // look for `dir/filename'
      String name = dir + filename;
      struct stat s;
      if (stat(name.cc(), &s) >= 0)
	return name;
      // look for `dir/subdir/filename'
      if (subdir) {
	name = dir + subdir + filename;
	if (stat(name.cc(), &s) >= 0)
	  return name;
      }
    }
    
    if (colon < 0) return String();
    path = path.substring(colon + 1);
  }
}

static String
path_find_file(const String &filename, const char *path_variable,
	       const String &default_path)
{
  const char *path = getenv(path_variable);
  if (path)
    return path_find_file_2(filename, path, default_path, 0);
  else
    return path_find_file_2(filename, default_path, "", 0);
}

static String
clickpath_find_file(const String &filename, const char *subdir,
		    const String &default_path, ErrorHandler *errh = 0)
{
  const char *path = getenv("CLICKPATH");
  String s;
  if (path)
    s = path_find_file_2(filename, path, default_path, subdir);
  else
    s = path_find_file_2(filename, default_path, "", 0);
  if (!s && errh) {
    errh->message("cannot find file `click.o'");
    errh->fatal("in CLICKPATH or `%s'", String(default_path).cc());
  }
  return s;
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

struct Classifion {
  int yes;
  int no;
  int offset;
  union {
    unsigned char c[4];
    unsigned u;
  } mask;
  union {
    unsigned char c[4];
    unsigned u;
  } value;
};
  
static void
analyze_classifier(RouterT *r, int classifier_ei, ErrorHandler *errh)
{
  // count number of output ports
  Vector<String> args;
  cp_argvec(r->econfiguration(classifier_ei), args);
  int noutputs = args.size();

  // set up new router
  RouterT nr;
  int classifier_nti = nr.get_type_index("Classifier");
  int idle_nti = nr.get_type_index("Idle");
  
  int classifier_nei =
    nr.get_eindex(r->ename(classifier_ei), classifier_nti,
		  r->econfiguration(classifier_ei));
  
  int idle_nei = nr.get_anon_eindex(idle_nti);
  nr.add_connection(idle_nei, 0, 0, classifier_nei);
  for (int i = 0; i < noutputs; i++)
    nr.add_connection(classifier_nei, i, 0, idle_nei);

  // copy AlignmentInfos
  int alignmentinfo_ti = r->get_type_index("AlignmentInfo");
  int alignmentinfo_nti = nr.get_type_index("AlignmentInfo");
  int nelements = r->nelements();
  for (int i = 0; i < nelements; i++)
    if (r->etype(i) == alignmentinfo_ti)
      nr.get_eindex(r->ename(i), alignmentinfo_nti, r->econfiguration(i));

  // get the resulting program from user-level `click'
  String router_str = nr.configuration_string();
  String handler = r->ename(classifier_ei) + ".program";
  String program = get_string_from_process(click_binary + " -h " + handler + " -q", router_str, errh);

  // parse the program
  Vector<Classifion> cls;
  int safe_length = -1;
  int output_everything = -1;
  int align_offset = -1;
  while (program) {
    // find step
    int newline = program.find_left('\n');
    String step = program.substring(0, newline);
    program = program.substring(newline + 1);
    // check for many things
    if (isdigit(step[0])) {
      // real step
      Classifion e;
      int crap, pos;
      int v[4], m[4];
      sscanf(step, "%d %d/%2x%2x%2x%2x%%%2x%2x%2x%2x yes->%n",
	     &crap, &e.offset, &v[0], &v[1], &v[2], &v[3],
	     &m[0], &m[1], &m[2], &m[3], &pos);
      for (int i = 0; i < 4; i++) {
	e.value.c[i] = v[i];
	e.mask.c[i] = m[i];
      }
      // read yes destination
      step = step.substring(pos);
      if (step[0] == '[') {
	sscanf(step, "[%d] no->%n", &e.yes, &pos);
	e.yes = -e.yes;
      } else
	sscanf(step, "step %d no->%n", &e.yes, &pos);
      // read no destination
      step = step.substring(pos);
      if (step[0] == '[') {
	sscanf(step, "[%d]", &e.no);
	e.no = -e.no;
      } else
	sscanf(step, "step %d", &e.no);
      // push expr onto list
      cls.push_back(e);
    } else if (sscanf(step, "all->[%d]", &output_everything))
      /* nada */;
    else if (sscanf(step, "safe length %d", &safe_length))
      /* nada */;
    else if (sscanf(step, "alignment offset %d", &align_offset))
      /* nada */;
  }

  // output corresponding code
  if (output_everything >= 0) {
    if (output_everything < noutputs)
      printf("  output(%d).push(p);\n", output_everything);
    else
      printf("  p->kill();\n");
  } else {
    printf("  if (p->length() < %d)\n    return 0;\n", safe_length);
    if (cls.size() > 0)
      printf("  const unsigned *data = (const unsigned *)(p->data() - %d);\n", align_offset);
    for (int i = 0; i < cls.size(); i++) {
      Classifion &e = cls[i];
      printf(" step_%d:\n", i);
      
      bool switched = (e.yes == i + 1);
      int branch1 = (switched ? e.no : e.yes);
      int branch2 = (switched ? e.yes : e.no);
      
      printf("  if (data[%d] & 0x%xU %s 0x%xU)", e.offset/4, e.mask.u,
	     (switched ? "!=" : "=="), e.value.u);
      if (branch1 <= -noutputs)
	printf(" {\n    p->kill();\n    return;\n  }\n");
      else if (branch1 <= 0)
	printf(" {\n    output(%d).push(p);\n    return;\n  }\n", -branch1);
      else
	printf("\n    goto step_%d;\n", branch1);
      if (branch2 <= -noutputs)
	printf("  p->kill();\n  return;\n");
      else if (branch2 <= 0)
	printf("  output(%d).push(p);\n  return;\n", -branch2);
      else if (branch2 != i + 1)
	printf("  goto step_%d;\n", branch2);
    }
  }
}


RouterT *
read_router_file(const char *filename, ErrorHandler *errh)
{
  FILE *f;
  if (filename && strcmp(filename, "-") != 0) {
    f = fopen(filename, "r");
    if (!f) {
      errh->error("%s: %s", filename, strerror(errno));
      return 0;
    }
  } else {
    f = stdin;
    filename = "<stdin>";
  }
  
  FileLexerTSource lex_source(filename, f);
  LexerT lexer(errh);
  lexer.reset(&lex_source);
  while (lexer.ystatement()) ;
  RouterT *r = lexer.take_router();
  
  if (f != stdin) fclose(f);
  return r;
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
      printf("click-fastclassifier (Click) %s\n", VERSION);
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

  // find Click binary
  click_binary = clickpath_find_file("click", "bin", CLICK_BINDIR, errh);
  
  // find Classifiers
  Vector<int> classifiers;
  {
    int t = r->type_index("Classifier");
    if (t >= 0)
      for (int i = 0; i < r->nelements(); i++)
	if (r->etype(i) == t)
	  classifiers.push_back(i);
  }

  // write Classifier programs
  for (int i = 0; i < classifiers.size(); i++)
    analyze_classifier(r, classifiers[i], errh);
  
  return 0;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<Classifion>;

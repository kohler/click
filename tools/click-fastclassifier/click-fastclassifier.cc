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
#include "toolutils.hh"
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

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
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
`Click-fastclassifier' transforms a router configuration by replacing generic\n\
Classifier elements with specific generated code. The resulting configuration\n\
has both Click-language files and object files.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -o, --output FILE           Write output to FILE.\n\
  -k, --kernel                Create Linux kernel module code (on by default).\n\
  -u, --user                  Create user-level code (on by default).\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static String
click_mktmpdir(ErrorHandler *errh = 0)
{
  String tmpdir;
  if (const char *path = getenv("TMPDIR"))
    tmpdir = path;
#ifdef P_tmpdir
  else if (P_tmpdir)
    tmpdir = P_tmpdir;
#endif
  else
    tmpdir = "/tmp";
  
  int uniqueifier = getpid();
  while (1) {
    String tmpsubdir = tmpdir + "/clicktmp" + String(uniqueifier);
    int result = mkdir(tmpsubdir.cc(), 0700);
    if (result >= 0)
      return tmpsubdir;
    if (result < 0 && errno != EEXIST) {
      if (errh)
	errh->fatal("cannot create temporary directory: %s", strerror(errno));
      return String();
    }
    uniqueifier++;
  }
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

static String
translate_class_name(const String &s)
{
  StringAccum sa;
  for (int i = 0; i < s.length(); i++)
    if (s[i] == '_')
      sa << "_u";
    else if (s[i] == '@')
      sa << "_a";
    else if (s[i] == '/')
      sa << "_s";
    else
      sa << s[i];
  return sa.take_string();
}

static void
write_checked_program(FILE *f, const Vector<Classifion> &cls,
		      int noutputs, int safe_length, int align_offset)
{
  fprintf(f, "  const unsigned *data = (const unsigned *)(p->data() - %d);\n", align_offset);
  fprintf(f, "  int l = p->length();\n");
  fprintf(f, "  assert(l < %d);\n", safe_length);

  for (int i = 0; i < cls.size(); i++) {
    const Classifion &e = cls[i];
    
    int want_l = e.offset + 4;
    if (!e.mask.c[3]) {
      want_l--;
      if (!e.mask.c[2]) {
	want_l--;
	if (!e.mask.c[1])
	  want_l--;
      }
    }
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    fprintf(f, " lstep_%d:\n", i);
    
    if (want_l >= safe_length) {
      branch2 = e.no;
      goto output_branch2;
    }
    
    if (switched)
      fprintf(f, "  if (l < %d || (data[%d] & 0x%xU) != 0x%xU)",
	      want_l, e.offset/4, e.mask.u, e.value.u);
    else
      fprintf(f, "  if (l >= %d && (data[%d] & 0x%xU) == 0x%xU)",
	      want_l, e.offset/4, e.mask.u, e.value.u);
    if (branch1 <= -noutputs)
      fprintf(f, " {\n    p->kill();\n    return;\n  }\n");
    else if (branch1 <= 0)
      fprintf(f, " {\n    output(%d).push(p);\n    return;\n  }\n", -branch1);
    else
      fprintf(f, "\n    goto lstep_%d;\n", branch1);
    
   output_branch2:
    if (branch2 <= -noutputs)
      fprintf(f, "  p->kill();\n  return;\n");
    else if (branch2 <= 0)
      fprintf(f, "  output(%d).push(p);\n  return;\n", -branch2);
    else if (branch2 != i + 1)
      fprintf(f, "  goto lstep_%d;\n", branch2);
  }
}

static void
write_unchecked_program(FILE *f, const Vector<Classifion> &cls,
			int noutputs, int align_offset)
{
  fprintf(f, "  const unsigned *data = (const unsigned *)(p->data() - %d);\n", align_offset);

  for (int i = 0; i < cls.size(); i++) {
    const Classifion &e = cls[i];
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    fprintf(f, " step_%d:\n", i);
      
    if (switched)
      fprintf(f, "  if ((data[%d] & 0x%xU) != 0x%xU)",
	      e.offset/4, e.mask.u, e.value.u);
    else
      fprintf(f, "  if ((data[%d] & 0x%xU) == 0x%xU)",
	      e.offset/4, e.mask.u, e.value.u);
    if (branch1 <= -noutputs)
      fprintf(f, " {\n    p->kill();\n    return;\n  }\n");
    else if (branch1 <= 0)
      fprintf(f, " {\n    output(%d).push(p);\n    return;\n  }\n", -branch1);
    else
      fprintf(f, "\n    goto step_%d;\n", branch1);
    if (branch2 <= -noutputs)
      fprintf(f, "  p->kill();\n  return;\n");
    else if (branch2 <= 0)
      fprintf(f, "  output(%d).push(p);\n  return;\n", -branch2);
    else if (branch2 != i + 1)
      fprintf(f, "  goto step_%d;\n", branch2);
  }
}

static Vector<String> gen_eclass_names;
static Vector<String> gen_cxxclass_names;

static void
analyze_classifier(RouterT *r, int classifier_ei, FILE *f, ErrorHandler *errh)
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
  String program = get_string_from_process(runclick_prog + " -h " + handler + " -q", router_str, errh);

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
  String class_name = "FastClassifier@@" + r->ename(classifier_ei);
  String cxx_name = translate_class_name(class_name);
  gen_eclass_names.push_back(class_name);
  gen_cxxclass_names.push_back(cxx_name);

  fprintf(f, "class %s : public Element { public:\n\
  %s() : Element(1, %d) { MOD_INC_USE_COUNT; }\n\
  ~%s() { MOD_DEC_USE_COUNT; }\n\
  const char *class_name() const { return \"%s\"; }\n\
  Processing default_processing() const { return PUSH; }\n\
  %s *clone() const { return new %s; }\n",
	  cxx_name.cc(), cxx_name.cc(), noutputs, cxx_name.cc(),
	  class_name.cc(), cxx_name.cc(), cxx_name.cc());
  
  if (output_everything >= 0) {
    fprintf(f, "  void push(int, Packet *);\n};\n\
void\n%s::push(int, Packet *p)\n{\n",
	    cxx_name.cc());
    if (output_everything < noutputs)
      fprintf(f, "  output(%d).push(p);\n", output_everything);
    else
      fprintf(f, "  p->kill();\n");
    fprintf(f, "}\n");
  } else {
    fprintf(f, "  void length_checked_push(Packet *);\n\
  inline void length_unchecked_push(Packet *);\n\
  void push(int, Packet *);\n};\n\
void\n%s::length_checked_push(Packet *p)\n{\n",
	    cxx_name.cc());
    write_checked_program(f, cls, noutputs, safe_length, align_offset);
    fprintf(f, "}\ninline void\n%s::length_unchecked_push(Packet *p)\n{\n",
	    cxx_name.cc());
    write_unchecked_program(f, cls, noutputs, align_offset);
    fprintf(f, "}\nvoid\n%s::push(int, Packet *p)\n{\n\
  if (p->length() < %d)\n    length_checked_push(p);\n\
  else\n    length_unchecked_push(p);\n}\n",
	    cxx_name.cc(), safe_length);
  }

  // add type to router `r' and change element
  int nclassifier_ti = r->get_type_index(class_name);
  ElementT &classifier_e = r->element(classifier_ei);
  classifier_e.type = nclassifier_ti;
  classifier_e.configuration = String();
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
  const char *output_file = 0;
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

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
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
  if (compile_kernel < 0 && compile_user < 0)
    compile_kernel = compile_user = 1;
  
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

  // find Click binaries
  runclick_prog = clickpath_find_file("click", "bin", CLICK_BINDIR, errh);
  String click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

  // find Classifiers
  Vector<int> classifiers;
  {
    int t = r->type_index("Classifier");
    if (t >= 0)
      for (int i = 0; i < r->nelements(); i++)
	if (r->etype(i) == t)
	  classifiers.push_back(i);
  }

  // quit early if no Classifiers
  if (classifiers.size() == 0) {
    fputs(r->configuration_string().cc(), outf);
    exit(0);
  }

  // find name of module
  String module_name = "fastclassifier";
  int uniqueifier = 1;
  while (1) {
    if (r->archive(module_name) < 0)
      break;
    uniqueifier++;
    module_name = "fastclassifier" + String(uniqueifier);
  }
  r->add_requirement(module_name);

  // create temporary directory
  String tmpdir = click_mktmpdir(errh);
  if (!tmpdir) exit(1);
  if (chdir(tmpdir.cc()) < 0)
    errh->fatal("cannot chdir to %s: %s", tmpdir.cc(), strerror(errno));
  
  // write C++ file
  String cxx_filename = module_name + "x.cc";
  FILE *f = fopen(cxx_filename, "w");
  if (!f)
    errh->fatal("%s: %s", cxx_filename.cc(), strerror(errno));
  fprintf(f, "#ifdef HAVE_CONFIG_H\n# include <config.h>\n#endif\n\
#include \"clickpackage.hh\"\n#include \"element.hh\"\n");
  
  // write Classifier programs
  for (int i = 0; i < classifiers.size(); i++)
    analyze_classifier(r, classifiers[i], f, errh);

  // write final text
  {
    int nclasses = gen_cxxclass_names.size();
    fprintf(f, "static int hatred_of_rebecca[%d];\n\
extern \"C\" int\ninit_module()\n{\n\
  click_provide(\"%s\");\n",
	    nclasses, module_name.cc());
    for (int i = 0; i < nclasses; i++)
      fprintf(f, "  hatred_of_rebecca[%d] = click_add_element_type(\"%s\", new %s);\n\
  MOD_DEC_USE_COUNT;\n",
	      i, gen_eclass_names[i].cc(), gen_cxxclass_names[i].cc());
    fprintf(f, "  return 0;\n}\nextern \"C\" void\ncleanup_module()\n{\n");
    for (int i = 0; i < nclasses; i++)
      fprintf(f, "  MOD_INC_USE_COUNT;\n\
  click_remove_element_type(hatred_of_rebecca[%d]);\n",
	      i);
    fprintf(f, "  click_unprovide(\"%s\");\n}\n", module_name.cc());
    fclose(f);
  }

  // compile kernel module
  if (compile_kernel > 0) {
    String compile_command = click_compile_prog + " --target=kernel --package=" + module_name + ".ko " + cxx_filename;
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
    String compile_command = click_compile_prog + " --target=user --package=" + module_name + ".uo -w " + cxx_filename;
    int compile_retval = system(compile_command.cc());
    if (compile_retval == 127)
      errh->fatal("could not run `%s'", compile_command.cc());
    else if (compile_retval < 0)
      errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
    else if (compile_retval != 0)
      errh->fatal("`%s' failed", compile_command.cc());
  }

  // read .cc and .?o files, add them to archive
  {
    ArchiveElement ae;
    ae.name = module_name + ".cc";
    ae.date = time(0);
    ae.uid = geteuid();
    ae.gid = getegid();
    ae.mode = 0600;
    ae.data = file_string(cxx_filename, errh);
    r->add_archive(ae);

    if (compile_kernel > 0) {
      ae.name = module_name + ".ko";
      ae.data = file_string(module_name + ".ko", errh);
      r->add_archive(ae);
    }
    
    if (compile_user > 0) {
      ae.name = module_name + ".uo";
      ae.data = file_string(module_name + ".uo", errh);
      r->add_archive(ae);
    }
  }
  
  // write configuration
  write_router_file(r, outf, errh);
  return 0;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<Classifion>;

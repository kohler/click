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

static Clp_Option options[] = {
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
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
  -k, --kernel                Create Linux kernel module code (on by default).\n\
  -u, --user                  Create user-level code (on by default).\n\
  -s, --source                Write source code only.\n\
  -c, --config                Write new configuration only.\n\
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

struct ProgramStep {
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

static bool
operator!=(const ProgramStep &s1, const ProgramStep &s2)
{
  return (s1.yes != s2.yes
	  || s1.no != s2.no
	  || s1.offset != s2.offset
	  || s1.mask.u != s2.mask.u
	  || s1.value.u != s2.value.u);
}

struct Classificand {
  int safe_length;
  int output_everything;
  int align_offset;
  int noutputs;
  Vector<ProgramStep> program;
  int type_index;
};

static bool
operator==(const Classificand &c1, const Classificand &c2)
{
  if (c1.safe_length != c2.safe_length
      || c1.output_everything != c2.output_everything
      || c1.noutputs != c2.noutputs
      || c1.align_offset != c2.align_offset
      || c1.program.size() != c2.program.size())
    return false;
  for (int i = 0; i < c1.program.size(); i++)
    if (c1.program[i] != c2.program[i])
      return false;
  return true;
}

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
write_checked_program(const Classificand &c, StringAccum &source)
{
  source << "  const unsigned *data = (const unsigned *)(p->data() - " << c.align_offset << ");\n";
  source << "  int l = p->length();\n  assert(l < " << c.safe_length << ");\n";

  for (int i = 0; i < c.program.size(); i++) {
    const ProgramStep &e = c.program[i];
    
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
    source << " lstep_" << i << ":\n";
    
    if (want_l >= c.safe_length) {
      branch2 = e.no;
      goto output_branch2;
    }
    
    if (switched)
      source << "  if (l < " << want_l << " || (data[" << e.offset/4
	     << "] & " << e.mask.u << "U) != " << e.value.u << "U)";
    else
      source << "  if (l >= " << want_l << " && (data[" << e.offset/4
	     << "] & " << e.mask.u << "U) == " << e.value.u << "U)";
    if (branch1 <= -c.noutputs)
      source << " {\n    p->kill();\n    return;\n  }\n";
    else if (branch1 <= 0)
      source << " {\n    output(" << -branch1 << ").push(p);\n    return;\n  }\n";
    else
      source << "\n    goto lstep_" << branch1 << ";\n";
    
   output_branch2:
    if (branch2 <= -c.noutputs)
      source << "  p->kill();\n  return;\n";
    else if (branch2 <= 0)
      source << "  output(" << -branch2 << ").push(p);\n  return;\n";
    else if (branch2 != i + 1)
      source << "  goto lstep_" << branch2 << ";\n";
  }
}

static void
write_unchecked_program(const Classificand &c, StringAccum &source)
{
  source << "  const unsigned *data = (const unsigned *)(p->data() - "
	 << c.align_offset << ");\n";

  for (int i = 0; i < c.program.size(); i++) {
    const ProgramStep &e = c.program[i];
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    source << " step_" << i << ":\n";
      
    if (switched)
      source << "  if ((data[" << e.offset/4 << "] & " << e.mask.u
	     << "U) != " << e.value.u << "U)";
    else
      source << "  if ((data[" << e.offset/4 << "] & " << e.mask.u
	     << "U) == " << e.value.u << "U)";
    if (branch1 <= -c.noutputs)
      source << " {\n    p->kill();\n    return;\n  }\n";
    else if (branch1 <= 0)
      source << " {\n    output(" << -branch1 << ").push(p);\n    return;\n  }\n";
    else
      source << "\n    goto step_" << branch1 << ";\n";
    if (branch2 <= -c.noutputs)
      source << "  p->kill();\n  return;\n";
    else if (branch2 <= 0)
      source << "  output(" << -branch2 << ").push(p);\n  return;\n";
    else if (branch2 != i + 1)
      source << "  goto step_" << branch2 << ";\n";
  }
}

static Vector<String> gen_eclass_names;
static Vector<String> gen_cxxclass_names;

static Vector<Classificand> all_programs;

static void
analyze_classifier(RouterT *r, int classifier_ei,
		   StringAccum &header, StringAccum &source,
		   ErrorHandler *errh)
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
		  r->econfiguration(classifier_ei), "");
  
  int idle_nei = nr.get_anon_eindex(idle_nti);
  nr.add_connection(idle_nei, 0, 0, classifier_nei);
  for (int i = 0; i < noutputs; i++)
    nr.add_connection(classifier_nei, i, i, idle_nei);

  // copy AlignmentInfos
  int alignmentinfo_ti = r->get_type_index("AlignmentInfo");
  int alignmentinfo_nti = nr.get_type_index("AlignmentInfo");
  int nelements = r->nelements();
  for (int i = 0; i < nelements; i++)
    if (r->etype(i) == alignmentinfo_ti)
      nr.get_eindex(r->ename(i), alignmentinfo_nti, r->econfiguration(i), "");

  // get the resulting program from user-level `click'
  String router_str = nr.configuration_string();
  String handler = r->ename(classifier_ei) + ".program";
  String program = get_string_from_process(runclick_prog + " -h " + handler + " -q", router_str, errh);

  // parse the program
  Classificand c;
  c.safe_length = c.output_everything = c.align_offset = -1;
  c.noutputs = noutputs;
  while (program) {
    // find step
    int newline = program.find_left('\n');
    String step = program.substring(0, newline);
    program = program.substring(newline + 1);
    // check for many things
    if (isdigit(step[0])) {
      // real step
      ProgramStep e;
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
      c.program.push_back(e);
    } else if (sscanf(step, "all->[%d]", &c.output_everything))
      /* nada */;
    else if (sscanf(step, "safe length %d", &c.safe_length))
      /* nada */;
    else if (sscanf(step, "alignment offset %d", &c.align_offset))
      /* nada */;
  }

  // search for an existing fast classifier with the same program
  for (int i = 0; i < all_programs.size(); i++)
    if (c == all_programs[i]) {
      ElementT &classifier_e = r->element(classifier_ei);
      classifier_e.type = all_programs[i].type_index;
      classifier_e.configuration = String();
      return;
    }
  
  // output corresponding code
  String class_name = "FastClassifier@@" + r->ename(classifier_ei);
  String cxx_name = translate_class_name(class_name);
  gen_eclass_names.push_back(class_name);
  gen_cxxclass_names.push_back(cxx_name);

  header << "class " << cxx_name << " : public Element { public:\n  "
	 << cxx_name << "() : Element(1, " << noutputs
	 << ") { MOD_INC_USE_COUNT; }\n  ~"
	 << cxx_name << "() { MOD_DEC_USE_COUNT; }\n\
  const char *class_name() const { return \"" << class_name << "\"; }\n\
  Processing default_processing() const { return PUSH; }\n  "
	 << cxx_name << " *clone() const { return new " << cxx_name << "; }\n";

  if (c.output_everything >= 0) {
    header << "  void push(int, Packet *);\n};\n";
    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    if (c.output_everything < noutputs)
      source << "  output(" << c.output_everything << ").push(p);\n";
    else
      source << "  p->kill();\n";
    source << "}\n";
  } else {
    header << "  void length_checked_push(Packet *);\n\
  inline void length_unchecked_push(Packet *);\n\
  void push(int, Packet *);\n};\n";
    source << "void\n" << cxx_name << "::length_checked_push(Packet *p)\n{\n";
    write_checked_program(c, source);
    source << "}\ninline void\n" << cxx_name
	   << "::length_unchecked_push(Packet *p)\n{\n";
    write_unchecked_program(c, source);
    source << "}\nvoid\n" << cxx_name << "::push(int, Packet *p)\n{\n\
  if (p->length() < " << c.safe_length << ")\n    length_checked_push(p);\n\
  else\n    length_unchecked_push(p);\n}\n";
  }

  // add type to router `r' and change element
  c.type_index = r->get_type_index(class_name);
  ElementT &classifier_e = r->element(classifier_ei);
  classifier_e.type = c.type_index;
  classifier_e.configuration = String();
  all_programs.push_back(c);
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
  int compile_kernel = -1;
  int compile_user = -1;
  bool source_only = false;
  bool config_only = false;
  
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
  if (compile_kernel < 0 && compile_user < 0) {
#ifdef HAVE_LINUXMODULE_TARGET
    compile_kernel = compile_user = 1;
#else
    compile_user = 1;
#endif
  }
  
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
  int nclassifiers = classifiers.size();

  // quit early if no Classifiers
  if (nclassifiers == 0) {
    if (source_only)
      errh->message("no Classifiers in router");
    else
      write_router_file(r, outf, errh);
    exit(0);
  }

  // find name of package
  String package_name = "fastclassifier";
  int uniqueifier = 1;
  while (1) {
    if (r->archive_index(package_name) < 0)
      break;
    uniqueifier++;
    package_name = "fastclassifier" + String(uniqueifier);
  }
  r->add_requirement(package_name);

  // create C++ files
  StringAccum header, source;
  header << "#ifndef CLICKSOURCE_" << package_name << "_HH\n"
	 << "#define CLICKSOURCE_" << package_name << "_HH\n"
	 << "#include \"clickpackage.hh\"\n#include \"element.hh\"\n";
  source << "#ifdef HAVE_CONFIG_H\n# include <config.h>\n#endif\n"
	 << "#include \"" << package_name << ".hh\"\n";
  
  // write Classifier programs
  for (int i = 0; i < nclassifiers; i++)
    analyze_classifier(r, classifiers[i], header, source, errh);

  // write final text
  {
    header << "#endif\n";
    int nclasses = gen_cxxclass_names.size();
    source << "static int hatred_of_rebecca[" << nclasses << "];\n"
	   << "extern \"C\" int\ninit_module()\n{\n\
  click_provide(\""
	   << package_name << "\");\n";
    for (int i = 0; i < nclasses; i++)
      source << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	     << gen_eclass_names[i] << "\", new "
	     << gen_cxxclass_names[i] << ");\n\
  MOD_DEC_USE_COUNT;\n";
    source << "  return 0;\n}\nextern \"C\" void\ncleanup_module()\n{\n";
    for (int i = 0; i < nclasses; i++)
      source << "  MOD_INC_USE_COUNT;\n\
  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
    source << "  click_unprovide(\"" << package_name << "\");\n}\n";
  }

  // open `f' for C++ output
  if (source_only) {
    fwrite(header.data(), 1, header.length(), outf);
    fwrite(source.data(), 1, source.length(), outf);
    if (outf != stdout)
      fclose(outf);
    exit(0);
  } else if (config_only) {
    String config = r->configuration_string();
    fwrite(config.data(), 1, config.length(), outf);
    if (outf != stdout)
      fclose(outf);
    exit(0);
  }

  // otherwise, compile files
  if (compile_kernel > 0 || compile_user > 0) {
    // create temporary directory
    String tmpdir = click_mktmpdir(errh);
    if (!tmpdir) exit(1);
    if (chdir(tmpdir.cc()) < 0)
      errh->fatal("cannot chdir to %s: %s", tmpdir.cc(), strerror(errno));
    
    String filename = package_name + ".hh";
    FILE *f = fopen(filename, "w");
    if (!f)
      errh->fatal("%s: %s", filename.cc(), strerror(errno));
    fwrite(header.data(), 1, header.length(), f);
    fclose(f);

    String cxx_filename = package_name + ".cc";
    f = fopen(cxx_filename, "w");
    if (!f)
      errh->fatal("%s: %s", cxx_filename.cc(), strerror(errno));
    fwrite(source.data(), 1, source.length(), f);
    fclose(f);
    
    // compile kernel module
    if (compile_kernel > 0) {
      String compile_command = click_compile_prog + " --target=kernel --package=" + package_name + ".ko " + cxx_filename;
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
      String compile_command = click_compile_prog + " --target=user --package=" + package_name + ".uo -w " + cxx_filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	errh->fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	errh->fatal("`%s' failed", compile_command.cc());
    }
  }

  // read .cc and .?o files, add them to archive
  {
    ArchiveElement ae = init_archive_element(package_name + ".cc", 0600);
    ae.data = source.take_string();
    r->add_archive(ae);

    ae.name = package_name + ".hh";
    ae.data = header.take_string();
    r->add_archive(ae);

    if (compile_kernel > 0) {
      ae.name = package_name + ".ko";
      ae.data = file_string(ae.name, errh);
      r->add_archive(ae);
    }
    
    if (compile_user > 0) {
      ae.name = package_name + ".uo";
      ae.data = file_string(ae.name, errh);
      r->add_archive(ae);
    }
  }

  // add elementmap to archive
  {
    if (r->archive_index("elementmap") < 0)
      r->add_archive(init_archive_element("elementmap", 0600));
    ArchiveElement &ae = r->archive("elementmap");
    StringAccum sa;
    for (int i = 0; i < gen_eclass_names.size(); i++)
      sa << gen_eclass_names[i] << '\t' << gen_cxxclass_names[i] << '\t'
	 << package_name << ".hh\n";
    ae.data += sa.take_string();
  }
  
  // write configuration
  write_router_file(r, outf, errh);
  return 0;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<ProgramStep>;
template class Vector<Classificand>;

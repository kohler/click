/*
 * click-fastclassifier.cc -- specialize Click classifiers
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "click-fastclassifier.hh"
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
#define COMBINE_OPT		309
#define COMPILE_OPT		310

static Clp_Option options[] = {
  { "classes", 0, COMPILE_OPT, 0, Clp_Negate },
  { "combine", 0, COMBINE_OPT, 0, Clp_Negate },
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
static String click_compile_prog;

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
  -f, --file FILE               Read router configuration from FILE.\n\
  -o, --output FILE             Write output to FILE.\n\
      --no-combine              Do not combine adjacent Classifiers.\n\
      --no-classes              Do not generate FastClassifier elements.\n\
  -k, --kernel                  Compile into Linux kernel binary package.\n\
  -u, --user                    Compile into user-level binary package.\n\
  -s, --source                  Write source code only.\n\
  -c, --config                  Write new configuration only.\n\
  -r, --reverse                 Reverse transformation.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


// Classifier related stuff

static bool
combine_classifiers(RouterT *router, int from_i, int from_port, int to_i)
{
  int classifier_t = router->type_index("Classifier");
  assert(router->etype(from_i) == classifier_t && router->etype(to_i) == classifier_t);
  
  // find where `to_i' is heading for
  Vector<int> first_hop, second_hop;
  router->find_connection_vector_from(from_i, first_hop);
  router->find_connection_vector_from(to_i, second_hop);

  // check for weird configurations
  for (int i = 0; i < first_hop.size(); i++)
    if (first_hop[i] < 0)
      return false;
  for (int i = 0; i < second_hop.size(); i++)
    if (second_hop[i] < 0)
      return false;
  if (second_hop.size() == 0)
    return false;

  // combine configurations
  Vector<String> from_words, to_words;
  cp_argvec(router->econfiguration(from_i), from_words);
  cp_argvec(router->econfiguration(to_i), to_words);
  if (from_words.size() != first_hop.size()
      || to_words.size() != second_hop.size())
    return false;
  Vector<String> new_words;
  for (int i = 0; i < from_port; i++)
    new_words.push_back(from_words[i]);
  for (int i = 0; i < to_words.size(); i++)
    if (to_words[i] == "-")
      new_words.push_back(from_words[from_port]);
    else if (from_words[from_port] == "-")
      new_words.push_back(to_words[i]);
    else
      new_words.push_back(from_words[from_port] + " " + to_words[i]);
  for (int i = from_port + 1; i < from_words.size(); i++)
    new_words.push_back(from_words[i]);
  router->econfiguration(from_i) = cp_unargvec(new_words);

  // change connections
  router->kill_connection(first_hop[from_port]);
  for (int i = from_port + 1; i < first_hop.size(); i++)
    router->change_connection_from(first_hop[i], Hookup(from_i, i + to_words.size() - 1));
  const Vector<Hookup> &ht = router->hookup_to();
  for (int i = 0; i < second_hop.size(); i++)
    router->add_connection(Hookup(from_i, from_port + i), ht[second_hop[i]]);

  return true;
}

static bool
try_combine_classifiers(RouterT *router, int class_i)
{
  int classifier_t = router->type_index("Classifier");
  if (router->etype(class_i) != classifier_t)
    // cannot combine IPClassifiers yet
    return false;

  const Vector<Hookup> &hf = router->hookup_from();
  const Vector<Hookup> &ht = router->hookup_to();
  for (int i = 0; i < hf.size(); i++)
    if (hf[i].idx == class_i && router->etype(ht[i].idx) == classifier_t
	&& ht[i].port == 0) {
      // perform a combination
      if (combine_classifiers(router, class_i, hf[i].port, ht[i].idx)) {
	try_combine_classifiers(router, class_i);
	return true;
      }
    }

  return false;
}

static void
try_remove_classifiers(RouterT *router, Vector<int> &classifiers)
{
  for (int i = 0; i < classifiers.size(); i++) {
    Vector<Hookup> v;
    router->find_connections_to(Hookup(classifiers[i], 0), v);
    if (v.size() == 0) {
      router->kill_element(classifiers[i]);
      classifiers[i] = classifiers.back();
      classifiers.pop_back();
      i--;
    }
  }

  router->remove_dead_elements();
  router->compact_connections();
}


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
  while (!feof(p) && sa.length() < 20000) {
    int x = fread(sa.reserve(2048), 1, 2048, p);
    if (x > 0) sa.forward(x);
  }
  if (!feof(p))
    errh->warning("`%s' output too long, truncated", cmdline.cc());

  fclose(f);
  pclose(p);
  return sa.take_string();
}

/*
 * FastClassifier structures
 */

bool
operator!=(const Classifier_Insn &s1, const Classifier_Insn &s2)
{
  return (s1.yes != s2.yes
	  || s1.no != s2.no
	  || s1.offset != s2.offset
	  || s1.mask.u != s2.mask.u
	  || s1.value.u != s2.value.u);
}

bool
operator==(const Classifier_Insn &s1, const Classifier_Insn &s2)
{
  return !(s1 != s2);
}

bool
operator==(const Classifier_Program &c1, const Classifier_Program &c2)
{
  if (c1.type != c2.type
      || c1.safe_length != c2.safe_length
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

bool
operator!=(const Classifier_Program &c1, const Classifier_Program &c2)
{
  return !(c1 == c2);
}


/*
 * registering CIDs
 */

struct FastClassifier_Cid {
  String name;
  int guaranteed_packet_length;
  void (*checked_body)(const Classifier_Program &, StringAccum &);
  void (*unchecked_body)(const Classifier_Program &, StringAccum &);
  void (*push_body)(const Classifier_Program &, StringAccum &);
};

static HashMap<String, int> cid_name_map(-1);
static Vector<FastClassifier_Cid *> cids;

int
add_classifier_type(const String &name, int guaranteed_packet_length,
	void (*checked_body)(const Classifier_Program &, StringAccum &),
	void (*unchecked_body)(const Classifier_Program &, StringAccum &),
	void (*push_body)(const Classifier_Program &, StringAccum &))
{
  FastClassifier_Cid *cid = new FastClassifier_Cid;
  cid->name = name;
  cid->guaranteed_packet_length = guaranteed_packet_length;
  cid->checked_body = checked_body;
  cid->unchecked_body = unchecked_body;
  cid->push_body = push_body;
  cids.push_back(cid);
  cid_name_map.insert(cid->name, cids.size() - 1);
  return cids.size() - 1;
}


/*
 * translating Classifiers
 */

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

static Vector<String> gen_eclass_names;
static Vector<String> gen_cxxclass_names;
static Vector<String> old_configurations;

static Vector<int> program_map;
static Vector<Classifier_Program> all_programs;

static void
change_landmark(ElementT &classifier_e)
{
  int colon = classifier_e.landmark.find_right(':');
  if (colon >= 0)
    classifier_e.landmark = classifier_e.landmark.substring(0, colon)
      + "<click-fastclassifier>" + classifier_e.landmark.substring(colon);
  else
    classifier_e.landmark = classifier_e.landmark + "<click-fastclassifier>";
}

static void
copy_elements(RouterT *oldr, RouterT *newr, const String &tname)
{
  int old_ti = oldr->type_index(tname);
  if (old_ti >= 0) {
    int new_ti = newr->get_type_index(tname);
    int nelements = oldr->nelements();
    for (int i = 0; i < nelements; i++)
      if (oldr->etype(i) == old_ti)
	newr->get_eindex(oldr->ename(i), new_ti, oldr->econfiguration(i), "");
  }
}

static void
analyze_classifiers(RouterT *r, const Vector<int> &classifier_ei,
		    ErrorHandler *errh)
{
  // set up new router
  RouterT nr;
  int idle_nei = nr.get_anon_eindex(nr.get_type_index("Idle"));
  
  // copy AlignmentInfos and AddressInfos
  copy_elements(r, &nr, "AlignmentInfo");
  copy_elements(r, &nr, "AddressInfo");

  // copy all classifiers
  HashMap<String, int> classifier_map(-1);
  for (int i = 0; i < classifier_ei.size(); i++) {
    int c = classifier_ei[i];
    classifier_map.insert(r->ename(c), i);
    
    // check what kind it is
    int classifier_nti = nr.get_type_index(r->etype_name(c));

    // add new classifier and connections to idle_nei
    int classifier_nei =
      nr.get_eindex(r->ename(c), classifier_nti,
		    r->econfiguration(c), r->elandmark(c));
  
    nr.add_connection(idle_nei, i, 0, classifier_nei);
    // count number of output ports
    int noutputs = r->noutputs(c);
    for (int j = 0; j < noutputs; j++)
      nr.add_connection(classifier_nei, j, 0, idle_nei);
  }

  // get the resulting programs from user-level `click'
  String router_str = nr.configuration_string();
  String programs = get_string_from_process(runclick_prog + " -h '*.program' -q", router_str, errh);

  // parse the programs
  while (1) {

    // skip to next '.program' handler
    const char *data = programs.data();
    int first_handler = programs.find_left(".program:");
    if (first_handler < 0)
      break;
    int second_handler = programs.find_left(".program:", first_handler + 9);
    if (second_handler >= 0) {
      while (second_handler > first_handler + 9 && data[second_handler] != '\n')
	second_handler--;
      second_handler++;
    } else
      second_handler = programs.length();

    String element_name = programs.substring(0, first_handler);
    String program = programs.substring(first_handler + 10, second_handler - first_handler - 10);
    programs = programs.substring(second_handler);
    
    // check if valid handler
    int ci = classifier_map[element_name];
    if (ci < 0)
      continue;
    int cei = classifier_ei[ci];

    // yes: valid handler; now parse program
    Classifier_Program c;
    String classifier_tname = r->etype_name(cei);
    c.type = cid_name_map[classifier_tname];
    assert(c.type >= 0);
    
    c.safe_length = c.output_everything = c.align_offset = -1;
    c.noutputs = r->noutputs(cei);
    while (program) {
      // find step
      int newline = program.find_left('\n');
      String step = program.substring(0, newline);
      program = program.substring(newline + 1);
      // check for many things
      if (isdigit(step[0]) || isspace(step[0])) {
	// real step
	Classifier_Insn e;
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
    bool found_program = false;
    for (int i = 0; i < all_programs.size() && !found_program; i++)
      if (c == all_programs[i]) {
	program_map.push_back(i);
	found_program = true;
      }

    if (!found_program) {
      // set new names
      String class_name = "Fast" + classifier_tname + "@@" + r->ename(cei);
      String cxx_name = translate_class_name(class_name);
      c.type_index = r->get_type_index(class_name);
      
      // add new program
      all_programs.push_back(c);
      gen_eclass_names.push_back(class_name);
      gen_cxxclass_names.push_back(cxx_name);
      old_configurations.push_back(r->econfiguration(cei));
      program_map.push_back(all_programs.size() - 1);
    }
  }
}

static void
output_classifier_program(int which,
			  StringAccum &header, StringAccum &source,
			  ErrorHandler *)
{
  String cxx_name = gen_cxxclass_names[which];
  String class_name = gen_eclass_names[which];
  const Classifier_Program &c = all_programs[which];
  FastClassifier_Cid *cid = cids[c.type];
  
  header << "class " << cxx_name << " : public Element {\n\
  void devirtualize_all() { }\n\
 public:\n  "
	 << cxx_name << "() { set_ninputs(1); set_noutputs(" << c.noutputs
	 << "); MOD_INC_USE_COUNT; }\n  ~"
	 << cxx_name << "() { MOD_DEC_USE_COUNT; }\n\
  const char *class_name() const { return \"" << class_name << "\"; }\n\
  " << cxx_name << " *clone() const { return new " << cxx_name << "; }\n\
  const char *processing() const { return PUSH; }\n";

  if (c.output_everything >= 0) {
    header << "  void push(int, Packet *);\n};\n";
    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    if (c.output_everything < c.noutputs)
      source << "  output(" << c.output_everything << ").push(p);\n";
    else
      source << "  p->kill();\n";
    source << "}\n";
  } else {
    bool need_checked = (c.safe_length >= cid->guaranteed_packet_length);
    if (need_checked) {
      header << "  void length_checked_push(Packet *);\n";
      source << "void\n" << cxx_name << "::length_checked_push(Packet *p)\n{\n";
      cid->checked_body(c, source);
      source << "}\n";
    }
    
    header << "  inline void length_unchecked_push(Packet *);\n\
  void push(int, Packet *);\n};\n";
    source << "inline void\n" << cxx_name
	   << "::length_unchecked_push(Packet *p)\n{\n";
    cid->unchecked_body(c, source);
    source << "}\n";

    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    cid->push_body(c, source);
    source << "}\n";
  }
}


static void
compile_classifiers(RouterT *r, const String &package_name,
		    Vector<int> &classifiers,
		    bool compile_kernel, bool compile_user, ErrorHandler *errh)
{
  r->add_requirement(package_name);

  // create C++ files
  StringAccum header, source;
  header << "#ifndef CLICK_" << package_name << "_HH\n"
	 << "#define CLICK_" << package_name << "_HH\n"
	 << "#include <click/package.hh>\n#include <click/element.hh>\n";
  source << "#include <click/config.h>\n";
  source << "#include \"" << package_name << ".hh\"\n\
#include <click/glue.hh>\n";

  // analyze Classifiers into programs
  analyze_classifiers(r, classifiers, errh);
  
  // write Classifier programs
  for (int i = 0; i < all_programs.size(); i++)
    output_classifier_program(i, header, source, errh);

  // change element landmarks and types
  for (int i = 0; i < classifiers.size(); i++) {
    ElementT &classifier_e = r->element(classifiers[i]);
    const Classifier_Program &c = all_programs[program_map[i]];
    classifier_e.type = c.type_index;
    classifier_e.configuration = String();
    change_landmark(classifier_e);
  }
  
  // write final text
  {
    header << "#endif\n";
    int nclasses = gen_cxxclass_names.size();
    source << "static int hatred_of_rebecca[" << nclasses << "];\n"
	   << "extern \"C\" int\ninit_module()\n{\n\
  click_provide(\""
	   << package_name << "\");\n";
    
    for (int i = 0; i < nclasses; i++)
      source << "  CLICK_DMALLOC_REG(\"FC" << i << "  \");\n"
	     << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	     << gen_eclass_names[i] << "\", new "
	     << gen_cxxclass_names[i] << ");\n";
    
    source << "  CLICK_DMALLOC_REG(\"FCxx\");\n  while (MOD_IN_USE > 1)\n    MOD_DEC_USE_COUNT;\n  return 0;\n}\n";
    source << "extern \"C\" void\ncleanup_module()\n{\n";
    for (int i = 0; i < nclasses; i++)
      source << "  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
    source << "  click_unprovide(\"" << package_name << "\");\n}\n";
  }

  // compile files if required
  if (compile_kernel || compile_user) {
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
    if (compile_kernel) {
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
    if (compile_user) {
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

  // add .cc, .hh and .?o files to archive
  {
    ArchiveElement ae = init_archive_element(package_name + ".cc", 0600);
    ae.data = source.take_string();
    r->add_archive(ae);

    ae.name = package_name + ".hh";
    ae.data = header.take_string();
    r->add_archive(ae);

    if (compile_kernel) {
      ae.name = package_name + ".ko";
      ae.data = file_string(ae.name, errh);
      r->add_archive(ae);
    }
    
    if (compile_user) {
      ae.name = package_name + ".uo";
      ae.data = file_string(ae.name, errh);
      r->add_archive(ae);
    }
  }

  // add elementmap to archive
  {
    if (r->archive_index("elementmap.fastclassifier") < 0)
      r->add_archive(init_archive_element("elementmap.fastclassifier", 0600));
    ArchiveElement &ae = r->archive("elementmap.fastclassifier");
    ElementMap em(ae.data);
    String header_file = package_name + ".hh";
    for (int i = 0; i < gen_eclass_names.size(); i++)
      em.add(gen_eclass_names[i], gen_cxxclass_names[i], header_file, "h/h", "x/x");
    ae.data = em.unparse();
  }

  // add classifier configurations to archive
  {
    if (r->archive_index("fastclassifier_info") < 0)
      r->add_archive(init_archive_element("fastclassifier_info", 0600));
    ArchiveElement &ae = r->archive("fastclassifier_info");
    StringAccum sa;
    for (int i = 0; i < gen_eclass_names.size(); i++) {
      sa << gen_eclass_names[i] << '\t'
	 << cids[all_programs[i].type]->name << '\t'
	 << cp_quote(old_configurations[i]) << '\n';
    }
    ae.data += sa.take_string();
  }
}



static void
reverse_transformation(RouterT *r, ErrorHandler *)
{
  // parse fastclassifier_config
  if (r->archive_index("fastclassifier_info") < 0)
    return;
  ArchiveElement &fc_ae = r->archive("fastclassifier_info");
  Vector<String> click_names, old_type_names, configurations;
  parse_tabbed_lines(fc_ae.data, &click_names, &old_type_names,
		     &configurations, (void *)0);

  // prepare type_index_map : type_index -> configuration #
  Vector<int> type_index_map(r->ntypes(), -1);
  for (int i = 0; i < click_names.size(); i++) {
    int ti = r->type_index(click_names[i]);
    if (ti >= 0)
      type_index_map[ti] = i;
  }

  // change configuration
  for (int i = 0; i < r->nelements(); i++) {
    ElementT &e = r->element(i);
    int ti = type_index_map[e.type];
    if (ti >= 0) {
      e.configuration = configurations[ti];
      e.type = r->get_type_index(old_type_names[ti]);
    }
  }

  // remove requirements
  {
    Vector<String> requirements = r->requirements();
    for (int i = 0; i < requirements.size(); i++)
      if (requirements[i].substring(0, 14) == "fastclassifier")
	r->remove_requirement(requirements[i]);
  }
  
  // remove archive elements
  for (int i = 0; i < r->narchive(); i++) {
    ArchiveElement &ae = r->archive(i);
    if (ae.name.substring(0, 14) == "fastclassifier"
	|| ae.name == "elementmap.fastclassifier")
      ae.name = String();
  }
}

extern "C" {
void add_fast_classifiers_1();
void add_fast_classifiers_2();
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
  bool compile_kernel = false;
  bool compile_user = false;
  bool combine_classifiers = true;
  bool do_compile = true;
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
      printf("click-fastclassifier (Click) %s\n", CLICK_VERSION);
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

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case COMBINE_OPT:
      combine_classifiers = !clp->negated;
      break;
      
     case COMPILE_OPT:
      do_compile = !clp->negated;
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
  if (r)
    r->flatten(errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  if (source_only || config_only)
    compile_user = compile_kernel = false;

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // handle reverse case
  if (reverse) {
    reverse_transformation(r, errh);
    write_router_file(r, outf, errh);
    exit(0);
  }

  // install classifier handlers
  add_fast_classifiers_1();
  add_fast_classifiers_2();
  
  // find Click binaries
  runclick_prog = clickpath_find_file("click", "bin", CLICK_BINDIR, errh);
  click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

  // find Classifiers
  Vector<int> classifiers;
  for (int i = 0; i < r->nelements(); i++)
    if (cid_name_map[r->etype_name(i)] >= 0)
      classifiers.push_back(i);

  // quit early if no Classifiers
  if (classifiers.size() == 0) {
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
    if (r->archive_index(package_name + ".cc") < 0)
      break;
    uniqueifier++;
    package_name = "fastclassifier" + String(uniqueifier);
  }
  
  // try combining classifiers
  if (combine_classifiers) {
    bool any_combined = false;
    for (int i = 0; i < classifiers.size(); i++)
      any_combined |= try_combine_classifiers(r, classifiers[i]);
    if (any_combined)
      try_remove_classifiers(r, classifiers);
  }

  if (do_compile)
    compile_classifiers(r, package_name, classifiers,
			compile_kernel, compile_user, errh);

  // write output
  if (source_only) {
    if (r->archive_index(package_name + ".hh") < 0) {
      errh->error("no source code generated");
      exit(1);
    }
    const ArchiveElement &aeh = r->archive(package_name + ".hh");
    const ArchiveElement &aec = r->archive(package_name + ".cc");
    fwrite(aeh.data.data(), 1, aeh.data.length(), outf);
    fwrite(aec.data.data(), 1, aec.data.length(), outf);
  } else if (config_only) {
    String config = r->configuration_string();
    fwrite(config.data(), 1, config.length(), outf);
  } else
    write_router_file(r, outf, errh);
  
  exit(0);
}

// generate Vector template instance
#include <click/vector.cc>
template class Vector<Classifier_Insn>;
template class Vector<Classifier_Program>;

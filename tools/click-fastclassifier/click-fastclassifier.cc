/*
 * click-fastclassifier.cc -- specialize Click classifiers
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

// magic constants imported from Click itself
#define IPCLASSIFIER_TRANSP_FAKE_OFFSET 64

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

enum { AC_CLASSIFIER, AC_IPCLASSIFIER, AC_IPFILTER } ClassifierType;

struct Classificand {
  int type;
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
  if (c.type == AC_CLASSIFIER)
    source << "  const unsigned *data = (const unsigned *)(p->data() - "
	   << c.align_offset << ");\n  int l = p->length();\n";
  else if (c.type == AC_IPCLASSIFIER || c.type == AC_IPFILTER)
    source << "  const unsigned *ip_data = (const unsigned *)p->ip_header();\n\
  const unsigned *transp_data = (const unsigned *)p->transport_header();\n\
  int l = p->length() + " << IPCLASSIFIER_TRANSP_FAKE_OFFSET << " - p->transport_header_offset();\n";

  source << "  assert(l < " << c.safe_length << ");\n";

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
    
    int offset;
    String datavar;
    String length_check;
    if (c.type == AC_CLASSIFIER) {
      offset = e.offset/4;
      datavar = "data";
      length_check = "l < " + String(want_l);
    } else { // c.type == AC_IPCLASSIFIER || c.type == AC_IPFILTER
      if (e.offset >= IPCLASSIFIER_TRANSP_FAKE_OFFSET) {
	offset = (e.offset - IPCLASSIFIER_TRANSP_FAKE_OFFSET)/4;
	datavar = "transp_data";
	length_check = "l < " + String(want_l);
      } else {
	offset = e.offset/4;
	datavar = "ip_data";
	length_check = "false";
      }
    }
    
    if (want_l >= c.safe_length) {
      branch2 = e.no;
      goto output_branch2;
    }

    if (switched)
      source << "  if (" << length_check << " || ("
	     << datavar << "[" << offset << "] & "
	     << e.mask.u << "U) != " << e.value.u << "U)";
    else
      source << "  if (!(" << length_check << ") && ("
	     << datavar << "[" << offset << "] & "
	     << e.mask.u << "U) == " << e.value.u << "U)";
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
  if (c.type == AC_CLASSIFIER)
    source << "  const unsigned *data = (const unsigned *)(p->data() - "
	   << c.align_offset << ");\n";
  else if (c.type == AC_IPCLASSIFIER || c.type == AC_IPFILTER)
    source << "  const unsigned *ip_data = (const unsigned *)p->ip_header();\n\
  const unsigned *transp_data = (const unsigned *)p->transport_header();\n";

  for (int i = 0; i < c.program.size(); i++) {
    const ProgramStep &e = c.program[i];
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    source << " step_" << i << ":\n";

    int offset = 0;
    String datavar;
    if (c.type == AC_CLASSIFIER)
      offset = e.offset/4, datavar = "data";
    else { // c.type == AC_IPCLASSIFIER || c.type == AC_IPFILTER
      if (e.offset >= IPCLASSIFIER_TRANSP_FAKE_OFFSET)
	offset = (e.offset - IPCLASSIFIER_TRANSP_FAKE_OFFSET)/4, datavar = "transp_data";
      else
	offset = e.offset/4, datavar = "ip_data";
    }
    
    if (switched)
      source << "  if ((" << datavar << "[" << offset << "] & " << e.mask.u
	     << "U) != " << e.value.u << "U)";
    else
      source << "  if ((" << datavar << "[" << offset << "] & " << e.mask.u
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
static Vector<String> old_configurations;

static Vector<Classificand> all_programs;

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
analyze_classifier(RouterT *r, int classifier_ei,
		   StringAccum &header, StringAccum &source,
		   ErrorHandler *errh)
{
  // check what kind it is
  String classifier_tname = r->etype_name(classifier_ei);

  // count number of output ports
  int noutputs = r->noutputs(classifier_ei);

  // set up new router
  RouterT nr;
  int classifier_nti = nr.get_type_index(classifier_tname);
  int idle_nti = nr.get_type_index("Idle");
  
  int classifier_nei =
    nr.get_eindex(r->ename(classifier_ei), classifier_nti,
		  r->econfiguration(classifier_ei), r->elandmark(classifier_ei));
  
  int idle_nei = nr.get_anon_eindex(idle_nti);
  nr.add_connection(idle_nei, 0, 0, classifier_nei);
  for (int i = 0; i < noutputs; i++)
    nr.add_connection(classifier_nei, i, 0, idle_nei);

  // copy AlignmentInfos and AddressInfos
  copy_elements(r, &nr, "AlignmentInfo");
  copy_elements(r, &nr, "AddressInfo");

  // get the resulting program from user-level `click'
  String router_str = nr.configuration_string();
  String handler = r->ename(classifier_ei) + ".program";
  String program = get_string_from_process(runclick_prog + " -h " + handler + " -q", router_str, errh);

  // parse the program
  Classificand c;
  if (classifier_tname == "Classifier")
    c.type = AC_CLASSIFIER;
  else if (classifier_tname == "IPClassifier")
    c.type = AC_IPCLASSIFIER;
  else if (classifier_tname == "IPFilter")
    c.type = AC_IPFILTER;
  else
    assert(0);
  c.safe_length = c.output_everything = c.align_offset = -1;
  c.noutputs = noutputs;
  while (program) {
    // find step
    int newline = program.find_left('\n');
    String step = program.substring(0, newline);
    program = program.substring(newline + 1);
    // check for many things
    if (isdigit(step[0]) || isspace(step[0])) {
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
      change_landmark(classifier_e);
      return;
    }
  
  // output corresponding code
  String class_name = "Fast" + classifier_tname + "@@" + r->ename(classifier_ei);
  String cxx_name = translate_class_name(class_name);
  gen_eclass_names.push_back(class_name);
  gen_cxxclass_names.push_back(cxx_name);
  old_configurations.push_back(r->econfiguration(classifier_ei));

  header << "class " << cxx_name << " : public Element {\n\
  void devirtualize_all() { }\n\
 public:\n  "
	 << cxx_name << "() { set_ninputs(1); set_noutputs(" << noutputs
	 << "); MOD_INC_USE_COUNT; }\n  ~"
	 << cxx_name << "() { MOD_DEC_USE_COUNT; }\n\
  const char *class_name() const { return \"" << class_name << "\"; }\n\
  " << cxx_name << " *clone() const { return new " << cxx_name << "; }\n\
  const char *processing() const { return PUSH; }\n";

  if (c.output_everything >= 0) {
    header << "  void push(int, Packet *);\n};\n";
    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    if (c.output_everything < noutputs)
      source << "  output(" << c.output_everything << ").push(p);\n";
    else
      source << "  p->kill();\n";
    source << "}\n";
  } else {
    if (c.type == AC_CLASSIFIER || c.safe_length >= IPCLASSIFIER_TRANSP_FAKE_OFFSET) {
      header << "  void length_checked_push(Packet *);\n";
      source << "void\n" << cxx_name << "::length_checked_push(Packet *p)\n{\n";
      write_checked_program(c, source);
      source << "}\n";
    }
    header << "  inline void length_unchecked_push(Packet *);\n\
  void push(int, Packet *);\n};\n";
    source << "inline void\n" << cxx_name
	   << "::length_unchecked_push(Packet *p)\n{\n";
    write_unchecked_program(c, source);
    source << "}\n";
    if (c.type == AC_CLASSIFIER) {
      source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n\
  if (p->length() < " << c.safe_length << ")\n    length_checked_push(p);\n\
  else\n    length_unchecked_push(p);\n}\n";
    } else { // c.type == AC_IPCLASSIFIER || c.type == AC_IPFILTER
      if (c.safe_length >= IPCLASSIFIER_TRANSP_FAKE_OFFSET)
	source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n\
  if (p->length() + " << IPCLASSIFIER_TRANSP_FAKE_OFFSET << " - p->transport_header_offset() < " << c.safe_length << ")\n    length_checked_push(p);\n\
  else\n    length_unchecked_push(p);\n}\n";
      else
	source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n\
  length_unchecked_push(p);\n}\n";
    }
  }

  // add type to router `r' and change element
  c.type_index = r->get_type_index(class_name);
  ElementT &classifier_e = r->element(classifier_ei);
  classifier_e.type = c.type_index;
  classifier_e.configuration = String();
  change_landmark(classifier_e);
  all_programs.push_back(c);
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
    const StringMap &requirements = r->requirement_map();
    Vector<String> removers;
    for (StringMap::Iterator iter = requirements.first(); iter; iter++)
      if (iter.value() > 0 && iter.key().substring(0, 14) == "fastclassifier")
	removers.push_back(iter.key());
    for (int i = 0; i < removers.size(); i++)
      r->remove_requirement(removers[i]);
  }
  
  // remove archive elements
  for (int i = 0; i < r->narchive(); i++) {
    ArchiveElement &ae = r->archive(i);
    if (ae.name.substring(0, 14) == "fastclassifier")
      ae.name = String();
  }

  // modify elementmap
  if (r->archive_index("elementmap") >= 0) {
    ArchiveElement &ae = r->archive("elementmap");
    ElementMap em(ae.data);
    for (int i = 0; i < click_names.size(); i++)
      em.remove(click_names[i]);
    ae.data = em.unparse();
    if (!ae.data) ae.name = String();
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
  header << "#ifndef CLICKSOURCE_" << package_name << "_HH\n"
	 << "#define CLICKSOURCE_" << package_name << "_HH\n"
	 << "#include \"clickpackage.hh\"\n#include \"element.hh\"\n";
  source << "#ifdef HAVE_CONFIG_H\n# include <config.h>\n#endif\n";
  source << "#include \"" << package_name << ".hh\"\n\
#include \"glue.hh\"\n";
  
  // write Classifier programs
  for (int i = 0; i < classifiers.size(); i++)
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
      source << "  CLICK_DMALLOC_REG(\"FC" << i << "  \");\n"
	     << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	     << gen_eclass_names[i] << "\", new "
	     << gen_cxxclass_names[i] << ");\n\
  CLICK_DMALLOC_REG(\"FCxx\");\n  MOD_DEC_USE_COUNT;\n";
    
    source << "  return 0;\n}\nextern \"C\" void\ncleanup_module()\n{\n";
    for (int i = 0; i < nclasses; i++)
      source << "  MOD_INC_USE_COUNT;\n\
  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
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
    if (r->archive_index("elementmap") < 0)
      r->add_archive(init_archive_element("elementmap", 0600));
    ArchiveElement &ae = r->archive("elementmap");
    ElementMap em(ae.data);
    String header_file = package_name + ".hh";
    for (int i = 0; i < gen_eclass_names.size(); i++)
      em.add(gen_eclass_names[i], gen_cxxclass_names[i], header_file, "h/h");
    ae.data = em.unparse();
  }

  // add classifier configurations to archive
  {
    if (r->archive_index("fastclassifier_info") < 0)
      r->add_archive(init_archive_element("fastclassifier_info", 0600));
    ArchiveElement &ae = r->archive("fastclassifier_info");
    StringAccum sa;
    for (int i = 0; i < gen_eclass_names.size(); i++) {
      sa << gen_eclass_names[i] << '\t';
      switch (all_programs[i].type) {
       case AC_CLASSIFIER: sa << "Classifier\t"; break;
       case AC_IPCLASSIFIER: sa << "IPClassifier\t"; break;
       case AC_IPFILTER: sa << "IPFilter\t"; break;
      }
      sa << cp_quote(old_configurations[i]) << '\n';
    }
    ae.data += sa.take_string();
  }
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
      printf("click-fastclassifier (Click) %s\n", VERSION);
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
  if (!r || errh->nerrors() > 0)
    exit(1);
  r->flatten(errh);
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
  
  // find Click binaries
  runclick_prog = clickpath_find_file("click", "bin", CLICK_BINDIR, errh);
  click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

  // find Classifiers
  Vector<int> classifiers;
  {
    int t1 = r->get_type_index("Classifier");
    int t2 = r->get_type_index("IPClassifier");
    int t3 = r->get_type_index("IPFilter");
    for (int i = 0; i < r->nelements(); i++)
      if (r->etype(i) == t1 || r->etype(i) == t2 || r->etype(i) == t3)
	classifiers.push_back(i);
  }

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
#include "vector.cc"
template class Vector<ProgramStep>;
template class Vector<Classificand>;

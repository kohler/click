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
#include "clp.h"
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

static String::Initializer string_initializer;
static HashMap<String, int> elementinfo_map(-1);
static Vector<String> elementinfo_click_name;
static Vector<String> elementinfo_cxx_name;
static Vector<String> elementinfo_header_fn;
static Vector<String> elementinfo_includes;
static HashMap<String, int> header_file_map(-1);

static CxxInfo cxx_info;

static Vector<int> element_specialize;
static Vector<String> element_click_type;
static Vector<String> element_cxx_type;

static HashMap<String, int> specialized_declarations(0);

static void
elementinfo_insert(const String &click, const String &cxx, const String &fn)
{
  int i = elementinfo_click_name.size();
  elementinfo_click_name.push_back(click);
  elementinfo_cxx_name.push_back(cxx);
  elementinfo_header_fn.push_back(fn);
  elementinfo_includes.push_back(String());
  elementinfo_map.insert(click, i);
  
  int slash = fn.find_right('/');
  header_file_map.insert(fn.substring(slash < 0 ? 0 : slash + 1), i);
}

static void
parse_elementmap(const String &str)
{
  int p = 0;
  int len = str.length();
  const char *s = str.data();

  while (p < len) {
    
    // read a line
    while (p < len && (s[p] == ' ' || s[p] == '\t'))
      p++;

    // skip blank lines & comments
    if (p < len && !isspace(s[p]) && s[p] != '#') {

      // read Click name
      int p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      if (p >= len || (s[p] != ' ' && s[p] != '\t'))
	continue;
      String click_name = str.substring(p1, p - p1);

      // read C++ name
      while (p < len && (s[p] == ' ' || s[p] == '\t'))
	p++;
      p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      if (p >= len || (s[p] != ' ' && s[p] != '\t'))
	continue;
      String cxx_name = str.substring(p1, p - p1);

      // read header filename
      while (p < len && (s[p] == ' ' || s[p] == '\t'))
	p++;
      p1 = p;
      while (p < len && !isspace(s[p]))
	p++;
      String header_fn = str.substring(p1, p - p1);

      // append information
      elementinfo_insert(click_name, cxx_name, header_fn);
    }

    // skip past end of line
    while (p < len && s[p] != '\n' && s[p] != '\r' && s[p] != '\f' && s[p] != '\v')
      p++;
    p++;
  }
}

static void
read_source(const String &click_name,
	    Vector<int> &read_source_vec, ErrorHandler *errh)
{
  int einfo = elementinfo_map[click_name];
  if (einfo < 0 || read_source_vec[einfo] || !elementinfo_header_fn[einfo])
    return;
  
  String filename = elementinfo_header_fn[einfo];
  String file_text = file_string("/u/eddietwo/src/click/" + filename, errh);
  cxx_info.parse_file(file_text, true);
  if (filename.substring(-2) == "hh") {
    file_text = file_string("/u/eddietwo/src/click/" + filename.substring(0, -2) + "cc", errh);
    cxx_info.parse_file(file_text, false, &elementinfo_includes[einfo]);
  }
  read_source_vec[einfo] = 1;

  // now, read source for the element class's parents
  CxxClass *cxxc = cxx_info.find_class(elementinfo_cxx_name[einfo]);
  if (cxxc)
    for (int i = 0; i < cxxc->nparents(); i++) {
      const String &p = cxxc->parent(i)->name();
      if (p != "Element" && p != "TimedElement" && p != "UnlimitedElement")
	read_source(p, read_source_vec, errh);
    }
}

static String
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

static String
specialized_click_name(RouterT *router, int i)
{
  return router->etype_name(i) + "@@" + router->ename(i);
}

static void
specialize_element(RouterT *router, int eindex, ErrorHandler *errh)
{
  String old_click_name = router->etype_name(eindex);
  String new_click_name = element_click_type[eindex];
  String new_cxx_name = element_cxx_type[eindex];
  
  // find element source
  int einfo = elementinfo_map[old_click_name];
  if (einfo < 0) {
    errh->warning("cannot specialize class `%s'", old_click_name.cc());
    element_specialize[eindex] = 0;
    element_click_type[eindex] = old_click_name;
    element_cxx_type[eindex] = String();
    return;
  }
  String old_cxx_name = elementinfo_cxx_name[einfo];
  CxxClass *cxxc = cxx_info.find_class(old_cxx_name);
  if (!cxxc) {
    errh->warning("class `%s' not found in source code", old_cxx_name.cc());
    element_specialize[eindex] = 0;
    element_click_type[eindex] = old_click_name;
    element_cxx_type[eindex] = old_cxx_name;
    return;
  }
  
  cxxc->mark_reachable_rewritable();

  // silently don't specialize if there are no reachable rewritables
  int nreachable_rewritable = 0;
  for (int i = 0; !nreachable_rewritable && i < cxxc->nfunctions(); i++)
    if (cxxc->reachable_rewritable(i))
      nreachable_rewritable++;
  if (!nreachable_rewritable) {
    element_specialize[eindex] = 0;
    element_click_type[eindex] = old_click_name;
    element_cxx_type[eindex] = old_cxx_name;
    return;
  }

  // find information about how the element is used in context
  int ninputs = 0;
  int noutputs = 0;
  const Vector<Hookup> &hfrom = router->hookup_from();
  const Vector<Hookup> &hto = router->hookup_to();
  for (int i = 0; i < router->nhookup(); i++) {
    if (hfrom[i].idx == eindex && hfrom[i].port >= noutputs)
      noutputs = hfrom[i].port + 1;
    if (hto[i].idx == eindex && hto[i].port >= ninputs)
      ninputs = hto[i].port + 1;
  }
  
  // make new C++ class
  CxxClass *new_cxxc = cxx_info.make_class(new_cxx_name);
  new_cxxc->add_parent(cxxc);

  // add helper functions: constructor, destructor, class_name, is_a
  new_cxxc->defun(CxxFunction(new_cxx_name, true, "", "()",
			      " MOD_INC_USE_COUNT; ", ""));
  new_cxxc->defun(CxxFunction("~" + new_cxx_name, true, "", "()",
			      " MOD_DEC_USE_COUNT; ", ""));
  new_cxxc->defun(CxxFunction("class_name", true, "const char *", "() const",
			      String(" return \"") + new_click_name + "\"; ",
			      ""));
  new_cxxc->defun(CxxFunction("is_a", false, "bool", "(const char *n) const",
			      "\n  return (strcmp(n, \"" + new_click_name + "\") == 0\n\
	  || strcmp(n, \"" + old_click_name + "\") == 0\n\
	  || " + old_cxx_name + "::is_a(n));\n",
			      ""));
  // placeholders for pull_input and push_output
  new_cxxc->defun(CxxFunction("pull_input", true, "Packet *",
			      (ninputs ? "(int i) const" : "(int) const"),
			      "", ""));
  new_cxxc->defun(CxxFunction("push_output", true, "void",
			      (noutputs ? "(int i, Packet *p) const" : "(int, Packet *p) const"),
			      "", ""));

  // transfer reachable rewritable functions to new C++ class
  // with pattern replacements
  {
    String ninputs_pat = compile_pattern("ninputs()");
    String ninputs_repl = String(ninputs);
    String noutputs_pat = compile_pattern("noutputs()");
    String noutputs_repl = String(noutputs);
    String push_pat = compile_pattern("output(#0).push(#1)");
    String checked_push_pat = compile_pattern("checked_push_output(#0, #1)");
    String push_repl = "push_output(#0, #1)";
    String pull_pat = compile_pattern("input(#0).pull()");
    String pull_repl = "pull_input(#0)";
    for (int i = 0; i < cxxc->nfunctions(); i++)
      if (cxxc->reachable_rewritable(i)) {
	CxxFunction &new_fn = new_cxxc->defun(cxxc->function(i));
	new_fn.replace_expr(ninputs_pat, ninputs_repl);
	new_fn.replace_expr(noutputs_pat, noutputs_repl);
	new_fn.replace_expr(push_pat, push_repl);
	new_fn.replace_expr(checked_push_pat, push_repl);
	new_fn.replace_expr(pull_pat, pull_repl);
      }
  }
}

static String
mangle(String click_name, bool push)
{
  // find the class, possibly a superclass, that defines the right vf
  int einfo = elementinfo_map[click_name];
  String cxx_name = (einfo >= 0 ? elementinfo_cxx_name[einfo] : String());
  CxxClass *cxx_class = cxx_info.find_class(cxx_name);
  while (cxx_class) {
    if (cxx_class->find(push ? "push" : "pull")) // success!
      break;
    if (cxx_class->nparents() == 1)
      cxx_class = cxx_class->parent(0);
    else			// XXX multiple inheritance
      cxx_class = 0;
  }
  cxx_name = (cxx_class ? cxx_class->name() : "Element");

  // mangle function 
  StringAccum sa;
  sa << (push ? "push__" : "pull__") << String(cxx_name.length()) << cxx_name;
  sa << (push ? "iP6Packet" : "i");
  return sa.take_string();
}

static void
finish_specialize_element(RouterT *router, int eindex,
			  StringAccum &out)
{
  int einfo = elementinfo_map[router->etype_name(eindex)];
  CxxClass *cxxc = cxx_info.find_class(element_cxx_type[eindex]);
  
  // create mangled names of attached push and pull functions
  const Vector<Hookup> &hfrom = router->hookup_from();
  const Vector<Hookup> &hto = router->hookup_to();
  int nhook = router->nhookup();
  int ninputs = 0;
  int noutputs = 0;
  for (int i = 0; i < nhook; i++) {
    if (hfrom[i].idx == eindex && hfrom[i].port >= noutputs)
      noutputs = hfrom[i].port + 1;
    if (hto[i].idx == eindex && hto[i].port >= ninputs)
      ninputs = hto[i].port + 1;
  }
  
  Vector<String> input_function(ninputs, String());
  Vector<String> output_function(noutputs, String());
  Vector<String> input_symbol(ninputs, String());
  Vector<String> output_symbol(noutputs, String());
  Vector<int> input_port(ninputs, -1);
  Vector<int> output_port(noutputs, -1);
  for (int i = 0; i < router->nhookup(); i++) {
    if (hfrom[i].idx == eindex) {
      output_function[hfrom[i].port] =
	"specialized_push_" + element_cxx_type[hto[i].idx];
      output_symbol[hfrom[i].port] =
	mangle(element_click_type[hto[i].idx], true);
      output_port[hfrom[i].port] = hto[i].port;
    }
    if (hto[i].idx == eindex) {
      input_function[hto[i].port] =
	"specialized_pull_" + element_cxx_type[hfrom[i].idx];
      input_symbol[hto[i].port] =
	mangle(element_click_type[hfrom[i].idx], false);
      input_port[hto[i].port] = hfrom[i].port;
    }
  }

  // create input_pull and output_push bodies
  StringAccum sa;
  for (int i = 0; i < ninputs; i++)
    sa << "if (i == " << i << ") return " << input_function[i] << "("
       << "input(" << i << ").element(), " << input_port[i] << ");\n";
  sa << "return 0;";
  cxxc->find("pull_input")->set_body(sa.take_string());
  
  for (int i = 0; i < noutputs; i++)
    sa << "if (i == " << i << ") { " << output_function[i] << "("
       << "output(" << i << ").element(), " << output_port[i] << ", p); return; }\n";
  sa << "p->kill();";
  cxxc->find("push_output")->set_body(sa.take_string());

  // output
  if (elementinfo_header_fn[einfo])
    out << "#include \"" << elementinfo_header_fn[einfo] << "\"\n";
  for (int i = 0; i < ninputs; i++)
    if (!specialized_declarations[input_function[i]]) {
      out << "extern Packet *" << input_function[i] << "(Element *, int)"
	  << " asm (\"" << input_symbol[i] << "\");\n";
      specialized_declarations.insert(input_function[i], 1);
    }
  for (int i = 0; i < noutputs; i++)
    if (!specialized_declarations[output_function[i]]) {
      out << "extern void " << output_function[i] << "(Element *, int, Packet *)"
	  << " asm (\"" << output_symbol[i] << "\");\n";
      specialized_declarations.insert(output_function[i], 1);
    }
  
  cxxc->header_text(out);

  // must massage includes.
  // we may have something like `#include "element.hh"', relying on the
  // assumption that we are compiling `element.cc'. must transform this
  // to `#include "path/to/element.hh"'.
  // XXX this is probably not the best way to do this
  const String &includes = elementinfo_includes[einfo];
  const char *s = includes.data();
  int len = includes.length();
  for (int p = 0; p < len; ) {
    int start = p;
    int p2 = p;
    while (p2 < len && s[p2] != '\n' && s[p2] != '\r')
      p2++;
    while (p < p2 && isspace(s[p]))
      p++;
    if (p < p2 && s[p] == '#') {
      for (p++; p < p2 && isspace(s[p]); p++) ;
      if (p + 7 < p2 && strncmp(s+p, "include", 7) == 0) {
	for (p += 7; p < p2 && isspace(s[p]); p++) ;
	if (p < p2 && s[p] == '\"') {
	  int left = p + 1;
	  for (p++; p < p2 && s[p] != '\"'; p++) ;
	  String include = includes.substring(left, p - left);
	  int include_index = header_file_map[include];
	  if (include_index >= 0) {
	    out << "#include \"" << elementinfo_header_fn[include_index] << "\"\n";
	    p = p2 + 1;
	    continue;
	  }
	}
      }
    }
    out << includes.substring(start, p2 + 1 - start);
    p = p2 + 1;
  }
  
  cxxc->source_text(out);
}


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
  
  // find and parse `elementmap'
  {
    String elementmap_fn =
      clickpath_find_file("elementmap", "share", CLICK_SHAREDIR);
    if (!elementmap_fn)
      errh->warning("cannot find `elementmap' in CLICKPATH or `%s'", CLICK_SHAREDIR);
    else {
      String elementmap_text = file_string(elementmap_fn, errh);
      parse_elementmap(elementmap_text);
    }
  }

  // read router
  RouterT *router = read_router_file(router_file, errh);
  if (!router || errh->nerrors() > 0)
    exit(1);
  router->flatten(errh);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find Click binaries
  String click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

  // what to specialize? everything
  element_specialize.assign(router->nelements(), 1);

  // read source code for all relevant elements
  Vector<int> read_source_vec(elementinfo_click_name.size(), 0);
  for (int i = 0; i < router->nelements(); i++)
    read_source(router->etype_name(i), read_source_vec, errh);

  // create element types for all specialized classes
  for (int i = 0; i < router->nelements(); i++)
    if (element_specialize[i]) {
      String new_click_name = specialized_click_name(router, i);
      String new_cxx_name = click_to_cxx_name(new_click_name);
      element_click_type.push_back(new_click_name);
      element_cxx_type.push_back(new_cxx_name);
      elementinfo_insert(new_click_name, new_cxx_name, "*");
    } else {
      String click_name = router->etype_name(i);
      element_click_type.push_back(click_name);
      int einfo = elementinfo_map[click_name];
      element_cxx_type.push_back(einfo >= 0 ? elementinfo_cxx_name[einfo] : String());
    }

  // actually specialize
  for (int i = 0; i < router->nelements(); i++)
    if (element_specialize[i])
      specialize_element(router, i, errh);
  StringAccum out;
  out << "#ifdef HAVE_CONFIG_H\n\
# include <config.h>\n\
#endif\n\
#include \"clickpackage.hh\"\n";
  for (int i = 0; i < router->nelements(); i++)
    if (element_specialize[i])
      finish_specialize_element(router, i, out);
  
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

  // output boilerplate package stuff
  int nclasses = 0;
  for (int i = 0; i < router->nelements(); i++)
    if (element_specialize[i])
      nclasses++;
  out << "static int hatred_of_rebecca[" << nclasses << "];\n";

  // init_module()
  out << "extern \"C\" int\ninit_module()\n{\n\
  click_provide(\""
      << package_name
      << "\");\n";
  for (int i = 0, j = 0; i < router->nelements(); i++)
    if (element_specialize[i]) {
      out << "  hatred_of_rebecca[" << j << "] = click_add_element_type(\""
	  << element_click_type[i] << "\", new " << element_cxx_type[i]
	  << ");\n  MOD_DEC_USE_COUNT;\n";
      j++;
    }
  out << "  return 0;\n}\n";

  // cleanup_module()
  out << "extern \"C\" void\ncleanup_module()\n{\n";
  for (int i = 0, j = 0; i < router->nelements(); i++)
    if (element_specialize[i]) {
      out << "  MOD_INC_USE_COUNT;\n  click_remove_element_type(hatred_of_rebecca[" << j << "]);\n";
      j++;
    }
  out << "  click_unprovide(\"" << package_name << "\");\n}\n";
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
  for (int i = 0; i < router->nelements(); i++)
    if (element_specialize[i])
      router->element(i).type = router->get_type_index(element_click_type[i]);
  
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

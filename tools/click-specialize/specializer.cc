/*
 * specializer.{cc,hh} -- specializer
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
#include "specializer.hh"
#include "routert.hh"
#include "error.hh"
#include "toolutils.hh"
#include "straccum.hh"
#include <ctype.h>

Specializer::Specializer(RouterT *router)
  : _router(router), _nelements(router->nelements()),
    _ninputs(router->nelements(), 0), _noutputs(router->nelements(), 0),
    _specialize(router->nelements(), 1), // specialize everything by default
    _specializing_classes(1),
    _specialize_like(_nelements, -1),
    _etinfo_map(0), _header_file_map(-1)
{
  _etinfo.push_back(ElementTypeInfo());
  
  const Vector<Hookup> &hf = router->hookup_from();
  const Vector<Hookup> &ht = router->hookup_to();
  for (int i = 0; i < hf.size(); i++) {
    if (hf[i].port >= _noutputs[hf[i].idx])
      _noutputs[hf[i].idx] = hf[i].port + 1;
    if (ht[i].port >= _ninputs[ht[i].idx])
      _ninputs[ht[i].idx] = ht[i].port + 1;
  }
}

inline ElementTypeInfo &
Specializer::etype_info(int eindex)
{
  return type_info(_router->etype_name(eindex));
}

inline const ElementTypeInfo &
Specializer::etype_info(int eindex) const
{
  return type_info(_router->etype_name(eindex));
}

void
Specializer::add_type_info(const String &click_name, const String &cxx_name,
			   const String &header_file)
{
  ElementTypeInfo eti;
  eti.click_name = click_name;
  eti.cxx_name = cxx_name;
  eti.header_file = header_file;
  _etinfo.push_back(eti);

  int i = _etinfo.size() - 1;
  _etinfo_map.insert(click_name, i);

  if (header_file) {
    int slash = header_file.find_right('/');
    _header_file_map.insert(header_file.substring(slash < 0 ? 0 : slash + 1),
			    i);
  }
}

void
Specializer::parse_elementmap(const String &str)
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
      add_type_info(click_name, cxx_name, header_fn);
    }
    
    // skip past end of line
    while (p < len && s[p] != '\n' && s[p] != '\r' && s[p] != '\f' && s[p] != '\v')
      p++;
    p++;
  }
}

void
Specializer::read_source(ElementTypeInfo &etinfo, ErrorHandler *errh)
{
  if (!etinfo.click_name || etinfo.read_source)
    return;
  etinfo.read_source = true;
  if (!etinfo.header_file) {
    errh->warning("element class `%s' has no source file", etinfo.click_name.cc());
    return;
  }

  // parse source text
  String text, filename = etinfo.header_file;
  if (filename.substring(-2) == "hh") {
    if (_router->archive_index(filename) >= 0)
      text = _router->archive(filename).data;
    else
      text = file_string(CLICK_SHAREDIR "/src/" + filename);
    _cxxinfo.parse_file(text, true);
    filename = filename.substring(0, -2) + "cc";
  }
  if (_router->archive_index(filename) >= 0)
    text = _router->archive(filename).data;
  else
    text = file_string(CLICK_SHAREDIR "/src/" + filename);
  _cxxinfo.parse_file(text, false, &etinfo.includes);

  // now, read source for the element class's parents
  CxxClass *cxxc = _cxxinfo.find_class(etinfo.cxx_name);
  if (cxxc)
    for (int i = 0; i < cxxc->nparents(); i++) {
      const String &p = cxxc->parent(i)->name();
      if (p != "Element" && p != "TimedElement" && p != "UnlimitedElement")
	read_source(type_info(p), errh);
    }
}

void
Specializer::set_specializing_classes(const HashMap<String, int> &sp)
{
  _specializing_classes = sp;
}

int
Specializer::set_specialize_like(String e1, String e2, ErrorHandler *errh)
{
  int eindex1 = _router->eindex(e1);
  if (eindex1 < 0)
    return errh->error("no element named `%s'", e1.cc());
  int eindex2 = _router->eindex(e2);
  if (eindex2 < 0)
    return errh->error("no element named `%s'", e2.cc());
  _specialize_like[eindex1] = eindex2;
  return 0;
}

int
Specializer::check_specialize(int eindex, ErrorHandler *errh)
{
  if (_specialize[eindex] != -97)
    return _specialize[eindex];
  _specialize[eindex] = -1;
  
  // specialize like something else?
  if (_specialize_like[eindex] >= 0) {
    _specialize[eindex] = check_specialize(_specialize_like[eindex], errh);
    return _specialize[eindex];
  }
  
  // get type info
  ElementTypeInfo &old_eti = etype_info(eindex);
  if (!old_eti.click_name)
    return errh->warning("no information about element class `%s'",
			 _router->etype_name(eindex).cc());
  
  // belongs to a non-specialized class?
  int try_it = 1;
  if (_specializing_classes[old_eti.click_name] <= 0)
    try_it = 0;
  if (try_it == 0)
    return -1;

  // read source code
  if (!old_eti.read_source)
    read_source(old_eti, errh);
  CxxClass *old_cxxc = _cxxinfo.find_class(old_eti.cxx_name);
  if (!old_cxxc)
    return errh->warning("C++ class `%s' not found for element class `%s'",
			 old_eti.cxx_name.cc(), old_eti.click_name.cc());

  // don't specialize if there are no reachable functions
  if (!old_cxxc->find_should_rewrite())
    return -1;

  // if we reach here, we should definitely specialize
  SpecializedClass spc;
  spc.click_name = specialized_click_name(_router, eindex);
  spc.cxx_name = click_to_cxx_name(spc.click_name);
  spc.cxxc = 0;
  spc.eindex = eindex;
  add_type_info(spc.click_name, spc.cxx_name);
  _specials.push_back(spc);
  return (_specialize[eindex] = _specials.size() - 1);
}

void
Specializer::create_class(SpecializedClass &spc)
{
  assert(!spc.cxxc);
  int eindex = spc.eindex;

  // create new C++ class
  const ElementTypeInfo &old_eti = etype_info(eindex);
  CxxClass *old_cxxc = _cxxinfo.find_class(old_eti.cxx_name);
  CxxClass *new_cxxc = _cxxinfo.make_class(spc.cxx_name);
  assert(old_cxxc && new_cxxc);
  bool specialize_away = (old_cxxc->find("specialize_away") != 0);
  String parent_cxx_name = old_eti.cxx_name;
  if (specialize_away) {
    CxxClass *parent = old_cxxc->parent(0);
    new_cxxc->add_parent(parent);
    parent_cxx_name = parent->name();
  } else
    new_cxxc->add_parent(old_cxxc);
  spc.cxxc = new_cxxc;

  // add helper functions: constructor, destructor, class_name, cast
  if (specialize_away) {
    CxxFunction *f = old_cxxc->find(old_eti.cxx_name);
    CxxFunction &constructor = new_cxxc->defun
      (CxxFunction(spc.cxx_name, true, "", "()", f->body(), f->clean_body()));
    if (!constructor.find_expr(compile_pattern("MOD_INC_USE_COUNT")))
      constructor.set_body(constructor.body() + "\nMOD_INC_USE_COUNT; ");

    f = old_cxxc->find("~" + old_eti.cxx_name);
    CxxFunction &destructor = new_cxxc->defun
      (CxxFunction("~" + spc.cxx_name, true, "", "()", f->body(), f->clean_body()));
    if (!destructor.find_expr(compile_pattern("MOD_DEC_USE_COUNT")))
      destructor.set_body(destructor.body() + "\nMOD_DEC_USE_COUNT; ");
    
  } else {
    new_cxxc->defun
      (CxxFunction(spc.cxx_name, true, "", "()",
		   " MOD_INC_USE_COUNT; ", ""));
    new_cxxc->defun
      (CxxFunction("~" + spc.cxx_name, true, "", "()",
		   " MOD_DEC_USE_COUNT; ", ""));
  }
  new_cxxc->defun
    (CxxFunction("class_name", true, "const char *", "() const",
		 String(" return \"") + spc.click_name + "\"; ", ""));
  new_cxxc->defun
    (CxxFunction("clone", true, spc.cxx_name + " *", "() const",
		 String(" return new ") + spc.cxx_name + "; ", ""));
  new_cxxc->defun
    (CxxFunction("cast", false, "void *", "(const char *n)",
		 "\n  if (void *v = " + parent_cxx_name + "::cast(n))\n\
    return v;\n  else if (strcmp(n, \"" + spc.click_name + "\") == 0\n\
	  || strcmp(n, \"" + old_eti.click_name + "\") == 0)\n\
    return v;\n  else\n    return 0;\n", ""));
  // placeholders for pull_input and push_output
  new_cxxc->defun
    (CxxFunction("pull_input", false, "inline Packet *",
		 (_ninputs[eindex] ? "(int i) const" : "(int) const"),
		 "", ""));
  new_cxxc->defun
    (CxxFunction("push_output", false, "inline void",
		 (_noutputs[eindex] ? "(int i, Packet *p) const" : "(int, Packet *p) const"),
		 "", ""));
  new_cxxc->defun
    (CxxFunction("push_output_checked", false, "inline void",
		 (_noutputs[eindex] ? "(int i, Packet *p) const" : "(int, Packet *p) const"),
		 "", ""));

  // transfer reachable rewritable functions to new C++ class
  // with pattern replacements
  {
    String ninputs_pat = compile_pattern("ninputs()");
    String ninputs_repl = String(_ninputs[eindex]);
    String noutputs_pat = compile_pattern("noutputs()");
    String noutputs_repl = String(_noutputs[eindex]);
    String push_pat = compile_pattern("output(#0).push(#1)");
    String checked_push_pat = compile_pattern("checked_push_output(#0, #1)");
    String checked_push_repl = compile_pattern("push_output_checked(#0, #1)");
    String push_repl = "push_output(#0, #1)";
    String pull_pat = compile_pattern("input(#0).pull()");
    String pull_repl = "pull_input(#0)";
    bool any_checked_push = false, any_push = false, any_pull = false;
    for (int i = 0; i < old_cxxc->nfunctions(); i++)
      if (old_cxxc->should_rewrite(i)) {
	const CxxFunction &old_fn = old_cxxc->function(i);
	if (new_cxxc->find(old_fn.name())) // don't add again
	  continue;
	CxxFunction &new_fn = new_cxxc->defun(old_fn);
	while (new_fn.replace_expr(ninputs_pat, ninputs_repl)) ;
	while (new_fn.replace_expr(noutputs_pat, noutputs_repl)) ;
	while (new_fn.replace_expr(push_pat, push_repl))
	  any_push = true;
	while (new_fn.replace_expr(checked_push_pat, checked_push_repl))
	  any_checked_push = true;
	while (new_fn.replace_expr(pull_pat, pull_repl))
	  any_pull = true;
      }
    if (!any_push && !any_checked_push)
      new_cxxc->find("push_output")->kill();
    if (!any_checked_push)
      new_cxxc->find("push_output_checked")->kill();
    if (!any_pull)
      new_cxxc->find("pull_input")->kill();
  }
}

void
Specializer::do_simple_action(SpecializedClass &spc)
{
  CxxFunction *simple_action = spc.cxxc->find("simple_action");
  assert(simple_action);
  simple_action->kill();

  spc.cxxc->defun
    (CxxFunction("smaction", false, "inline Packet *", simple_action->args(),
		 simple_action->body(), simple_action->clean_body()));
  spc.cxxc->defun
    (CxxFunction("push", false, "void", "(int, Packet *p)",
		 "\n  if (Packet *q = smaction(p))\n\
    push_output(0, q);\n", ""));
  spc.cxxc->defun
    (CxxFunction("pull", false, "Packet *", "(int)",
		 "\n  Packet *p = pull_input(0);\n\
  return (p ? smaction(p) : 0);\n", ""));
  spc.cxxc->find("push_output")->unkill();
  spc.cxxc->find("pull_input")->unkill();
}

inline const String &
Specializer::enew_click_type(int i) const
{
  int j = _specialize[i];
  if (j < 0)
    return etype_info(i).click_name;
  else
    return _specials[j].click_name;
}

inline const String &
Specializer::enew_cxx_type(int i) const
{
  int j = _specialize[i];
  if (j < 0)
    return etype_info(i).cxx_name;
  else
    return _specials[j].cxx_name;
}

String
Specializer::emangle(int eindex, bool push) const
{
  // find the class, possibly a superclass, that defines the right vf
  CxxClass *cxx_class = _cxxinfo.find_class(enew_cxx_type(eindex));
  while (cxx_class) {
    if (cxx_class->find(push ? "push" : "pull")) // success!
      break;
    if (cxx_class->nparents() == 1)
      cxx_class = cxx_class->parent(0);
    else			// XXX multiple inheritance
      cxx_class = 0;
  }
  String cxx_name = (cxx_class ? cxx_class->name() : "Element");

  // mangle function 
  StringAccum sa;
  sa << (push ? "push__" : "pull__") << String(cxx_name.length()) << cxx_name;
  sa << (push ? "iP6Packet" : "i");
  return sa.take_string();
}

void
Specializer::create_connector_methods(SpecializedClass &spc)
{
  assert(spc.cxxc);
  int eindex = spc.eindex;
  CxxClass *cxxc = spc.cxxc;
  
  // create mangled names of attached push and pull functions
  const Vector<Hookup> &hf = _router->hookup_from();
  const Vector<Hookup> &ht = _router->hookup_to();
  int nhook = _router->nhookup();
  Vector<String> input_function(_ninputs[eindex], String());
  Vector<String> output_function(_noutputs[eindex], String());
  Vector<String> input_symbol(_ninputs[eindex], String());
  Vector<String> output_symbol(_noutputs[eindex], String());
  Vector<int> input_port(_ninputs[eindex], -1);
  Vector<int> output_port(_noutputs[eindex], -1);
  for (int i = 0; i < nhook; i++) {
    if (hf[i].idx == eindex) {
      output_function[hf[i].port] =
	"specialized_push_" + enew_cxx_type(ht[i].idx);
      output_symbol[hf[i].port] = emangle(ht[i].idx, true);
      output_port[hf[i].port] = ht[i].port;
    }
    if (ht[i].idx == eindex) {
      input_function[ht[i].port] =
	"specialized_pull_" + enew_cxx_type(hf[i].idx);
      input_symbol[ht[i].port] = emangle(hf[i].idx, false);
      input_port[ht[i].port] = hf[i].port;
    }
  }

  // create pull_input
  if (cxxc->find("pull_input")->alive()) {
    StringAccum sa;
    Vector<int> range1, range2;
    for (int i = 0; i < _ninputs[eindex]; i++)
      if (i > 0 && input_function[i] == input_function[i-1]
	  && input_port[i] == input_port[i-1])
	range2.back() = i;
      else {
	range1.push_back(i);
	range2.push_back(i);
      }
    for (int i = 0; i < range1.size(); i++) {
      int r1 = range1[i], r2 = range2[i];
      sa << "\n  ";
      if (i < range1.size() - 1) {
	if (r1 == r2)
	  sa << "if (i == " << r1 << ") ";
	else
	  sa << "if (i >= " << r1 << " && i <= " << r2 << ") ";
      }
      sa << "return " << input_function[r1] << "(input(i).element(), "
	 << input_port[r1] << ");";
    }
    if (range1.size() == 0)
      sa << "\n  return 0;";	// shut up warnings
    sa << "\n";
    cxxc->find("pull_input")->set_body(sa.take_string());
    
    // save function names
    for (int i = 0; i < _ninputs[eindex]; i++) {
      _specfunction_names.push_back(input_function[i]);
      _specfunction_symbols.push_back(input_symbol[i]);
    }
  }

  // create push_output
  if (cxxc->find("push_output")->alive()) {
    StringAccum sa;
    Vector<int> range1, range2;
    for (int i = 0; i < _noutputs[eindex]; i++)
      if (i > 0 && output_function[i] == output_function[i-1]
	  && output_port[i] == output_port[i-1])
	range2.back() = i;
      else {
	range1.push_back(i);
	range2.push_back(i);
      }
    for (int i = 0; i < range1.size(); i++) {
      int r1 = range1[i], r2 = range2[i];
      sa << "\n  ";
      if (i < range1.size() - 1) {
	if (r1 == r2)
	  sa << "if (i == " << r1 << ") ";
	else
	  sa << "if (i >= " << r1 << " && i <= " << r2 << ") ";
      }
      sa << "{ " << output_function[r1] << "(output(i).element(), "
	 << output_port[r1] << ", p); return; }";
    }
    sa << "\n";
    cxxc->find("push_output")->set_body(sa.take_string());
    
    sa.clear();
    sa << "\n  if (i < " << _noutputs[eindex] << ")\n    push_output(i, p);\n";
    sa << "  else\n    p->kill();\n";
    cxxc->find("push_output_checked")->set_body(sa.take_string());
    
    // save function names
    for (int i = 0; i < _noutputs[eindex]; i++) {
      _specfunction_names.push_back(output_function[i]);
      _specfunction_symbols.push_back(output_symbol[i]);
    }
  }
}

void
Specializer::specialize(ErrorHandler *errh)
{
  _specialize.assign(_nelements, -97);
  for (int i = 0; i < _nelements; i++)
    check_specialize(i, errh);

  for (int s = 0; s < _specials.size(); s++) {
    create_class(_specials[s]);
    if (_specials[s].cxxc->find("simple_action"))
      do_simple_action(_specials[s]);
  }

  for (int s = 0; s < _specials.size(); s++)
    create_connector_methods(_specials[s]);
}

void
Specializer::fix_elements()
{
  for (int i = 0; i < _nelements; i++)
    if (_specialize[i] >= 0) {
      SpecializedClass &spc = _specials[ _specialize[i] ];
      _router->element(i).type = _router->get_type_index(spc.click_name);
    }
}

void
Specializer::output_includes(const ElementTypeInfo &eti, StringAccum &out)
{
  // must massage includes.
  // we may have something like `#include "element.hh"', relying on the
  // assumption that we are compiling `element.cc'. must transform this
  // to `#include "path/to/element.hh"'.
  // XXX this is probably not the best way to do this
  const String &includes = eti.includes;
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
	  int include_index = _header_file_map[include];
	  if (include_index >= 0) {
	    out << "#include \"" << _etinfo[include_index].header_file << "\"\n";
	    p = p2 + 1;
	    continue;
	  }
	}
      }
    }
    out << includes.substring(start, p2 + 1 - start);
    p = p2 + 1;
  }
}

void
Specializer::output(StringAccum &out)
{
  // output headers
  for (int i = 0; i < _specials.size(); i++) {
    ElementTypeInfo &eti = etype_info(_specials[i].eindex);
    if (eti.header_file)
      out << "#include \"" << eti.header_file << "\"\n";
    _specials[i].cxxc->header_text(out);
  }

  // output functions
  HashMap<String, int> declared(0);
  for (int i = 0; i < _specfunction_names.size(); i++)
    if (!declared[_specfunction_names[i]]) {
      const String &name = _specfunction_names[i];
      const String &sym = _specfunction_symbols[i];
      if (sym[2] == 's')		// push
	out << "extern void " << name << "(Element *, int, Packet *) asm (\""
	    << sym << "\");\n";
      else
	out << "extern Packet *" << name << "(Element *, int) asm (\""
	    << sym << "\");\n";
      declared.insert(name, 1);
    }

  // output C++ code
  for (int i = 0; i < _specials.size(); i++) {
    SpecializedClass &spc = _specials[i];
    ElementTypeInfo &eti = etype_info(spc.eindex);
    output_includes(eti, out);
    spc.cxxc->source_text(out);
  }
}

void
Specializer::output_package(const String &package_name, StringAccum &out)
{
  // output boilerplate package stuff
  out << "static int hatred_of_rebecca[" << _specials.size() << "];\n";

  // init_module()
  out << "extern \"C\" int\ninit_module()\n{\n\
  click_provide(\""
      << package_name
      << "\");\n";
  for (int i = 0; i < _specials.size(); i++)
    out << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	<< _specials[i].click_name << "\", new " << _specials[i].cxx_name
	<< ");\n  MOD_DEC_USE_COUNT;\n";
  out << "  return 0;\n}\n";

  // cleanup_module()
  out << "extern \"C\" void\ncleanup_module()\n{\n";
  for (int i = 0; i < _specials.size(); i++)
    out << "  MOD_INC_USE_COUNT;\n  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
  out << "  click_unprovide(\"" << package_name << "\");\n";
  out << "}\n";
}

String
Specializer::output_new_elementmap(const String &filename) const
{
  StringAccum out;
  for (int i = 0; i < _specials.size(); i++)
    out << _specials[i].click_name << '\t' << _specials[i].cxx_name << '\t'
	<< filename << '\n';
  return out.take_string();
}

// Vector template instantiation
#include "vector.cc"
template class Vector<ElementTypeInfo>;
template class Vector<SpecializedClass>;

/*
 * specializer.{cc,hh} -- specializer
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include "specializer.hh"
#include "routert.hh"
#include <click/error.hh>
#include "toolutils.hh"
#include "elementmap.hh"
#include <click/straccum.hh>
#include "signature.hh"
#include <ctype.h>

Specializer::Specializer(RouterT *router, const ElementMap &em)
  : _router(router), _nelements(router->nelements()),
    _ninputs(router->nelements(), 0), _noutputs(router->nelements(), 0),
    _etinfo_map(0), _header_file_map(-1), _parsed_sources(-1)
{
  _etinfo.push_back(ElementTypeInfo());
  
  for (RouterT::iterator x = router->first_element(); x; x++) {
    _noutputs[x->idx()] = x->noutputs();
    _ninputs[x->idx()] = x->ninputs();
  }

  // prepare from element map
  for (ElementMap::TraitsIterator x = em.first_element(); x; x++) {
    const Traits &e = x.value();
    add_type_info(e.name, e.cxx, e.header_file, em.source_directory(e));
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
			   const String &header_file, const String &source_dir)
{
  ElementTypeInfo eti;
  eti.click_name = click_name;
  eti.cxx_name = cxx_name;
  eti.header_file = header_file;
  eti.source_directory = source_dir;
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
ElementTypeInfo::locate_header_file(RouterT *for_archive, ErrorHandler *errh)
{
  if (!found_header_file) {
    if (!source_directory && for_archive->archive_index(header_file) >= 0)
      found_header_file = header_file;
    else if (String found = clickpath_find_file(header_file, 0, source_directory))
      found_header_file = found;
    else {
      errh->warning("can't locate header file \"%s\"", header_file.cc());
      found_header_file = header_file;
    }
  }
}

void
Specializer::parse_source_file(ElementTypeInfo &etinfo,
			       bool do_header, String *includes)
{
  String fn = etinfo.header_file;
  if (!do_header && fn.substring(-3) == ".hh")
    fn = etinfo.header_file.substring(0, -3) + ".cc";
  
  // don't parse a source file twice
  if (_parsed_sources[fn] < 0) {
    String text;
    if (!etinfo.source_directory && _router->archive_index(fn) >= 0) {
      text = _router->archive(fn).data;
      if (do_header)
	etinfo.found_header_file = fn;
    } else if (String found = clickpath_find_file(fn, 0, etinfo.source_directory)) {
      text = file_string(found);
      if (do_header)
	etinfo.found_header_file = found;
    }
    _cxxinfo.parse_file(text, do_header, includes);
    _parsed_sources.insert(fn, 1);
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
  if (filename.substring(-3) == ".hh")
    parse_source_file(etinfo, true, 0);
  parse_source_file(etinfo, false, &etinfo.includes);

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
Specializer::check_specialize(int eindex, ErrorHandler *errh)
{
  int sp = _specialize[eindex];
  if (_specials[sp].eindex > SPCE_NOT_DONE)
    return;
  _specials[sp].eindex = SPCE_NOT_SPECIAL;
  
  // get type info
  ElementTypeInfo &old_eti = etype_info(eindex);
  if (!old_eti.click_name) {
    errh->warning("no information about element class `%s'",
		  _router->etype_name(eindex).cc());
    return;
  }
  
  // read source code
  if (!old_eti.read_source)
    read_source(old_eti, errh);
  CxxClass *old_cxxc = _cxxinfo.find_class(old_eti.cxx_name);
  if (!old_cxxc) {
    errh->warning("C++ class `%s' not found for element class `%s'",
		  old_eti.cxx_name.cc(), old_eti.click_name.cc());
    return;
  }

  // don't specialize if there are no reachable functions
  SpecializedClass &spc = _specials[sp];
  spc.old_click_name = old_eti.click_name;
  spc.eindex = eindex;
  if (!old_cxxc->find_should_rewrite()) {
    spc.click_name = spc.old_click_name;
    spc.cxx_name = old_eti.cxx_name;
  } else {
    spc.click_name = specialized_click_name(_router->element(eindex));
    spc.cxx_name = click_to_cxx_name(spc.click_name);
    add_type_info(spc.click_name, spc.cxx_name, String(), String());
  }
}

bool
Specializer::create_class(SpecializedClass &spc)
{
  assert(!spc.cxxc);
  int eindex = spc.eindex;
  if (spc.click_name == spc.old_click_name)
    return false;

  // create new C++ class
  const ElementTypeInfo &old_eti = etype_info(eindex);
  CxxClass *old_cxxc = _cxxinfo.find_class(old_eti.cxx_name);
  CxxClass *new_cxxc = _cxxinfo.make_class(spc.cxx_name);
  assert(old_cxxc && new_cxxc);
  bool specialize_away = (old_cxxc->find("devirtualize_all") != 0);
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
    return (Element *)this;\n  else\n    return 0;\n", ""));
  // placeholders for input_pull and output_push
  new_cxxc->defun
    (CxxFunction("input_pull", false, "inline Packet *",
		 (_ninputs[eindex] ? "(int i) const" : "(int) const"),
		 "", ""));
  new_cxxc->defun
    (CxxFunction("output_push", false, "inline void",
		 (_noutputs[eindex] ? "(int i, Packet *p) const" : "(int, Packet *p) const"),
		 "", ""));
  new_cxxc->defun
    (CxxFunction("output_push_checked", false, "inline void",
		 (_noutputs[eindex] ? "(int i, Packet *p) const" : "(int, Packet *p) const"),
		 "", ""));
  new_cxxc->defun
    (CxxFunction("never_devirtualize", true, "void", "()", "", ""));

  // transfer reachable rewritable functions to new C++ class
  // with pattern replacements
  {
    String ninputs_pat = compile_pattern("ninputs()");
    String ninputs_repl = String(_ninputs[eindex]);
    String noutputs_pat = compile_pattern("noutputs()");
    String noutputs_repl = String(_noutputs[eindex]);
    String push_pat = compile_pattern("output(#0).push(#1)");
    String push_repl = "output_push(#0, #1)";
    String checked_push_pat = compile_pattern("checked_output_push(#0, #1)");
    String checked_push_repl = compile_pattern("output_push_checked(#0, #1)");
    String pull_pat = compile_pattern("input(#0).pull()");
    String pull_repl = "input_pull(#0)";
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
      new_cxxc->find("output_push")->kill();
    if (!any_checked_push)
      new_cxxc->find("output_push_checked")->kill();
    if (!any_pull)
      new_cxxc->find("input_pull")->kill();
  }

  return true;
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
    output_push(0, q);\n", ""));
  spc.cxxc->defun
    (CxxFunction("pull", false, "Packet *", "(int)",
		 "\n  Packet *p = input_pull(0);\n\
  return (p ? smaction(p) : 0);\n", ""));
  spc.cxxc->find("output_push")->unkill();
  spc.cxxc->find("input_pull")->unkill();
}

inline const String &
Specializer::enew_cxx_type(int i) const
{
  int j = _specialize[i];
  return _specials[j].cxx_name;
}

void
Specializer::create_connector_methods(SpecializedClass &spc)
{
  assert(spc.cxxc);
  int eindex = spc.eindex;
  CxxClass *cxxc = spc.cxxc;
  
  // create mangled names of attached push and pull functions
  const Vector<ConnectionT> &conn = _router->connections();
  int nhook = _router->nconnections();
  Vector<String> input_class(_ninputs[eindex], String());
  Vector<String> output_class(_noutputs[eindex], String());
  Vector<int> input_port(_ninputs[eindex], -1);
  Vector<int> output_port(_noutputs[eindex], -1);
  for (int i = 0; i < nhook; i++) {
    if (conn[i].from_idx() == eindex) {
      output_class[conn[i].from_port()] = enew_cxx_type(conn[i].to_idx());
      output_port[conn[i].from_port()] = conn[i].to_port();
    }
    if (conn[i].to_idx() == eindex) {
      input_class[conn[i].to_port()] = enew_cxx_type(conn[i].from_idx());
      input_port[conn[i].to_port()] = conn[i].from_port();
    }
  }

  // create input_pull
  if (cxxc->find("input_pull")->alive()) {
    StringAccum sa;
    Vector<int> range1, range2;
    for (int i = 0; i < _ninputs[eindex]; i++)
      if (i > 0 && input_class[i] == input_class[i-1]
	  && input_port[i] == input_port[i-1])
	range2.back() = i;
      else {
	range1.push_back(i);
	range2.push_back(i);
      }
    for (int i = 0; i < range1.size(); i++) {
      int r1 = range1[i], r2 = range2[i];
      if (!input_class[r1])
	continue;
      sa << "\n  ";
      if (r1 == r2)
	sa << "if (i == " << r1 << ") ";
      else
	sa << "if (i >= " << r1 << " && i <= " << r2 << ") ";
      sa << "return ((" << input_class[r1] << " *)input(i).element())->"
	 << input_class[r1] << "::pull(" << input_port[r1] << ");";
    }
    sa << "\n  return input(i).pull();\n";
    cxxc->find("input_pull")->set_body(sa.take_string());
  }

  // create output_push
  if (cxxc->find("output_push")->alive()) {
    StringAccum sa;
    Vector<int> range1, range2;
    for (int i = 0; i < _noutputs[eindex]; i++)
      if (i > 0 && output_class[i] == output_class[i-1]
	  && output_port[i] == output_port[i-1])
	range2.back() = i;
      else {
	range1.push_back(i);
	range2.push_back(i);
      }
    for (int i = 0; i < range1.size(); i++) {
      int r1 = range1[i], r2 = range2[i];
      if (!output_class[r1])
	continue;
      sa << "\n  ";
      if (r1 == r2)
	sa << "if (i == " << r1 << ") ";
      else
	sa << "if (i >= " << r1 << " && i <= " << r2 << ") ";
      sa << "{ ((" << output_class[r1] << " *)output(i).element())->"
	 << output_class[r1] << "::push(" << output_port[r1]
	 << ", p); return; }";
    }
    sa << "\n  output(i).push(p);\n";
    cxxc->find("output_push")->set_body(sa.take_string());
    
    sa.clear();
    sa << "\n  if (i < " << _noutputs[eindex] << ")\n    output_push(i, p);\n";
    sa << "  else\n    p->kill();\n";
    cxxc->find("output_push_checked")->set_body(sa.take_string());
  }
}

void
Specializer::specialize(const Signatures &sigs, ErrorHandler *errh)
{
  // decide what is to be specialized
  _specialize = sigs.signature_ids();
  SpecializedClass spc;
  spc.eindex = SPCE_NOT_DONE;
  _specials.assign(sigs.nsignatures(), spc);
  _specials[0].eindex = SPCE_NOT_SPECIAL;
  for (int i = 0; i < _nelements; i++)
    check_specialize(i, errh);

  // actually do the work
  for (int s = 0; s < _specials.size(); s++) {
    if (create_class(_specials[s]) && _specials[s].cxxc->find("simple_action"))
      do_simple_action(_specials[s]);
  }

  for (int s = 0; s < _specials.size(); s++)
    if (_specials[s].special())
      create_connector_methods(_specials[s]);
}

void
Specializer::fix_elements()
{
  for (int i = 0; i < _nelements; i++) {
    SpecializedClass &spc = _specials[ _specialize[i] ];
    if (spc.special())
      _router->element(i)->set_type(_router->get_type(spc.click_name));
  }
}

void
Specializer::output_includes(ElementTypeInfo &eti, StringAccum &out)
{
  // don't write includes twice for the same class
  if (eti.wrote_includes)
    return;
  
  // must massage includes.
  // we may have something like `#include "element.hh"', relying on the
  // assumption that we are compiling `element.cc'. must transform this
  // to `#include "path/to/element.hh"'.
  // XXX this is probably not the best way to do this
  const String &includes = eti.includes;
  const char *s = includes.data();
  int len = includes.length();

  // skip past '#ifndef X\n#define X' (sort of)
  int p = 0;
  while (p < len && isspace(s[p]))
    p++;
  if (p + 7 < len && strncmp(s + p, "#ifndef", 7) == 0) {
    int next = p + 7;
    for (; next < len && s[next] != '\n'; next++)
      /* nada */;
    if (next + 8 < len && strncmp(s + next + 1, "#define", 7) == 0) {
      for (p = next + 8; p < len && s[p] != '\n'; p++)
	/* nada */;
    }
  }

  // now collect includes
  while (p < len) {
    int start = p;
    int p2 = p;
    while (p2 < len && s[p2] != '\n' && s[p2] != '\r')
      p2++;
    while (p < p2 && isspace(s[p]))
      p++;

    if (p < p2 && s[p] == '#') {
      // we have a preprocessing directive!
      
      // skip space after '#'
      for (p++; p < p2 && isspace(s[p]); p++)
	/* nada */;

      // check for '#include'
      if (p + 7 < p2 && strncmp(s+p, "include", 7) == 0) {
	
	// find what is "#include"d
	for (p += 7; p < p2 && isspace(s[p]); p++)
	  /* nada */;

	// interested in "user includes", not <system includes>
	if (p < p2 && s[p] == '\"') {
	  int left = p + 1;
	  for (p++; p < p2 && s[p] != '\"'; p++)
	    /* nada */;
	  String include = includes.substring(left, p - left);
	  int include_index = _header_file_map[include];
	  if (include_index >= 0) {
	    if (!_etinfo[include_index].found_header_file)
	      _etinfo[include_index].locate_header_file(_router, ErrorHandler::default_handler());
	    out << "#include \"" << _etinfo[include_index].found_header_file << "\"\n";
	    p = p2 + 1;
	    continue;		// don't use previous #include text
	  }
	}
      }
      
    }

    out << includes.substring(start, p2 + 1 - start);
    p = p2 + 1;
  }

  eti.wrote_includes = true;
}

void
Specializer::output(StringAccum &out)
{
  // output headers
  for (int i = 0; i < _specials.size(); i++) {
    SpecializedClass &spc = _specials[i];
    if (spc.eindex >= 0) {
      ElementTypeInfo &eti = etype_info(spc.eindex);
      if (eti.found_header_file)
	out << "#include \"" << eti.found_header_file << "\"\n";
      if (spc.special())
	spc.cxxc->header_text(out);
    }
  }

  // output C++ code
  for (int i = 0; i < _specials.size(); i++) {
    SpecializedClass &spc = _specials[i];
    if (spc.special()) {
      ElementTypeInfo &eti = etype_info(spc.eindex);
      output_includes(eti, out);
      spc.cxxc->source_text(out);
    }
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
    if (_specials[i].special())
      out << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	  << _specials[i].click_name << "\", new " << _specials[i].cxx_name
	  << ");\n";
  out << "  while (MOD_IN_USE > 1)\n    MOD_DEC_USE_COUNT;\n  return 0;\n}\n";

  // cleanup_module()
  out << "extern \"C\" void\ncleanup_module()\n{\n";
  for (int i = 0; i < _specials.size(); i++)
    if (_specials[i].special())
      out << "  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
  out << "  click_unprovide(\"" << package_name << "\");\n";
  out << "}\n";
}

void
Specializer::output_new_elementmap(const ElementMap &full_em, ElementMap &em,
				   const String &filename, const String &requirements) const
{
  for (int i = 0; i < _specials.size(); i++)
    if (_specials[i].special()) {
      const Traits &e = full_em.traits(_specials[i].old_click_name);
      em.add(_specials[i].click_name, _specials[i].cxx_name,
	     filename, e.processing_code(), e.flow_code(),
	     e.flags, requirements + _specials[i].old_click_name, String());
    }
}

// Vector template instantiation
#include <click/vector.cc>
template class Vector<ElementTypeInfo>;
template class Vector<SpecializedClass>;

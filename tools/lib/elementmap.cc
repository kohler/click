// -*- c-basic-offset: 4 -*-
/*
 * elementmap.{cc,hh} -- an element map class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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

#include <click/straccum.hh>
#include <click/bitvector.hh>
#include "routert.hh"
#include "lexert.hh"
#include "elementmap.hh"
#include "toolutils.hh"
#include <click/confparse.hh>

ElementMap::ElementMap()
    : _uid_map(0)
{
    _e.push_back(Elt());
    _def_srcdir.push_back(String());
    _def_compile_flags.push_back(String());
    _def_package.push_back(String());
}

ElementMap::ElementMap(const String &str)
    : _uid_map(0)
{
    _e.push_back(Elt());
    _def_srcdir.push_back(String());
    _def_compile_flags.push_back(String());
    _def_package.push_back(String());
    parse(str);
}

const char *
ElementMap::driver_name(int d)
{
    if (d == DRIVER_LINUXMODULE)
	return "linuxmodule";
    else if (d == DRIVER_USERLEVEL)
	return "userlevel";
    else if (d == DRIVER_BSDMODULE)
	return "bsdmodule";
    else
	return "??";
}

const char *
ElementMap::driver_requirement(int d)
{
    if (d == DRIVER_LINUXMODULE)
	return "linuxmodule";
    else if (d == DRIVER_USERLEVEL)
	return "userlevel";
    else if (d == DRIVER_BSDMODULE)
	return "bsdmodule";
    else
	return "";
}

static bool
requirement_contains(const String &req, const String &n)
{
    int pos = 0;
    while ((pos = req.find_left(n)) >= 0) {
	int rpos = pos + n.length();
	// XXX should be more careful about '|' bars
	if ((pos == 0 || isspace(req[pos - 1]) || req[pos - 1] == '|')
	    && (rpos == req.length() || isspace(req[rpos]) || req[rpos] == '|'))
	    return true;
	pos = rpos;
    }
    return false;  
}

bool
ElementMap::Elt::requires(const String &n) const
{
    if (!requirements)
	return false;
    else
	return requirement_contains(requirements, n);
}

bool
ElementMap::Elt::provides(const String &n) const
{
    if (n == name)
	return true;
    if (!provisions)
	return false;
    else
	return requirement_contains(provisions, n);
}

int
ElementMap::Elt::flag_value(int flag) const
{
    const unsigned char *data = reinterpret_cast<const unsigned char *>(flags.data());
    int len = flags.length();
    for (int i = 0; i < len; i++) {
	if (data[i] == flag) {
	    if (i < len - 1 && isdigit(data[i+1])) {
		int value = 0;
		for (i++; i < len && isdigit(data[i]); i++)
		    value = 10*value + data[i] - '0';
		return value;
	    } else
		return 1;
	} else
	    while (i < len && data[i] != ',')
		i++;
    }
    return -1;
}

const String &
ElementMap::source_directory(const ElementClassT *c) const
{
    return _def_srcdir[ elt(c).def_index ];
}

const String &
ElementMap::package(const ElementClassT *c) const
{
    return _def_package[ elt(c).def_index ];
}

int
ElementMap::get_driver_mask(const String &requirements)
{
    int driver_mask = 0;
    if (requirement_contains(requirements, "linuxmodule"))
	driver_mask |= 1 << DRIVER_LINUXMODULE;
    if (requirement_contains(requirements, "userlevel"))
	driver_mask |= 1 << DRIVER_USERLEVEL;
    if (requirement_contains(requirements, "bsdmodule"))
	driver_mask |= 1 << DRIVER_BSDMODULE;
    return (driver_mask ? driver_mask : ALL_DRIVERS);
}

int
ElementMap::add(const Elt &e)
{
    int i = _e.size();
    _e.push_back(e);

    Elt &my_e = _e.back();
    if (my_e.requirements)
	my_e.driver_mask = get_driver_mask(my_e.requirements);

    if (e.name) {
	ElementClassT *c = ElementClassT::default_class(e.name);
	my_e.uid = c->uid();
	my_e.uid_next = _uid_map[c->uid()];
	_uid_map.insert(c->uid(), i);
    }

    return i;
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code,
		const String &flow_code, const String &flags,
		const String &requirements, const String &provisions)
{
    Elt e;
    e.name = click_name;
    e.cxx = cxx_name;
    e.header_file = header_file;
    e.processing_code = processing_code;
    e.flow_code = flow_code;
    e.flags = flags;
    e.requirements = requirements;
    e.provisions = provisions;
    return add(e);
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code,
		const String &flow_code)
{
    return add(click_name, cxx_name, header_file, processing_code, flow_code,
	       String(), String(), String());
}

void
ElementMap::remove(int i)
{
    if (i <= 0 || i >= _e.size())
	return;

    Elt &e = _e[i];
    int p = -1;
    for (int t = _uid_map[e.uid]; t > 0; p = t, t = _e[t].uid_next)
	/* nada */;
    if (p >= 0)
	_e[p].uid_next = e.uid_next;
    else if (e.uid >= 0)
	_uid_map.insert(e.uid, e.uid_next);

    e.uid = -1;
    e.name = e.cxx = String();
}

String *
ElementMap::Elt::component(int what)
{
    switch (what) {
      case D_CLASS:		return &name;
      case D_CXX_CLASS:		return &cxx;
      case D_HEADER_FILE:	return &header_file;
      case D_SOURCE_FILE:	return &source_file;
      case D_PROCESSING:	return &processing_code;
      case D_FLOW_CODE:		return &flow_code;
      case D_FLAGS:		return &flags;
      case D_REQUIREMENTS:	return &requirements;
      case D_PROVISIONS:	return &provisions;
      default:			return 0;
    }
}

int
ElementMap::parse_component(const String &s)
{
    if (s == "class")
	return D_CLASS;
    else if (s == "cxx_class")
	return D_CXX_CLASS;
    else if (s == "header_file")
	return D_HEADER_FILE;
    else if (s == "source_file")
	return D_SOURCE_FILE;
    else if (s == "processing")
	return D_PROCESSING;
    else if (s == "flow_code")
	return D_FLOW_CODE;
    else if (s == "requirements")
	return D_REQUIREMENTS;
    else if (s == "provisions")
	return D_PROVISIONS;
    else
	return D_NONE;
}

void
ElementMap::parse(const String &str, const String &package_name)
{
    int p, len = str.length();
    int endp = 0;

    int def_index = 0;
    if (package_name != _def_package[0]) {
	def_index = _def_srcdir.size();
	_def_srcdir.push_back(String());
	_def_compile_flags.push_back(String());
	_def_package.push_back(package_name);
    }

    // set up default data
    Vector<int> data;
    for (int i = D_FIRST_DEFAULT; i <= D_LAST_DEFAULT; i++)
	data.push_back(i);

    // loop over the lines
    for (p = 0; p < len; p = endp + 1) {
	// read a line
	endp = str.find_left('\n', p);
	if (endp < 0)
	    endp = str.length();
	String line = str.substring(p, endp - p);

	// break into words
	Vector<String> words;
	cp_spacevec(line, words);

	// skip blank lines & comments
	if (words.size() == 0 || words[0][0] == '#')
	    continue;

	// check for $sourcedir
	if (words[0] == "$sourcedir") {
	    if (words.size() == 2) {
		def_index = _def_srcdir.size();
		_def_srcdir.push_back(cp_unquote(words[1]));
		_def_compile_flags.push_back(_def_compile_flags[def_index - 1]);
		_def_package.push_back(package_name);
	    }

	} else if (words[0] == "$provides") {
	    for (int i = 1; i < words.size(); i++)
		_e[0].provisions += " " + cp_unquote(words[i]);

	} else if (words[0] == "$data") {
	    data.clear();
	    for (int i = 1; i < words.size(); i++)
		data.push_back(parse_component(cp_unquote(words[i])));

	} else if (words[0][0] != '$') {
	    // an actual line
	    Elt elt;
	    for (int i = 0; i < data.size() && i < words.size(); i++)
		if (String *sp = elt.component(data[i]))
		    *sp = cp_unquote(words[i]);
	    if (elt.provisions || elt.name) {
		elt.def_index = def_index;
		(void) add(elt);
	    }
	}
    }
}

void
ElementMap::parse(const String &str)
{
    parse(str, String());
}

String
ElementMap::unparse() const
{
    StringAccum sa;
    sa << "$data\tclass\tcxx_class\theader_file\tprocessing\tflow_code\tflags\trequirements\tprovisions\n";
    for (int i = 1; i < _e.size(); i++) {
	const Elt &e = _e[i];
	if (!e.name && !e.cxx)
	    continue;
	sa << cp_quote(e.name) << '\t'
	   << cp_quote(e.cxx) << '\t'
	   << cp_quote(e.header_file) << '\t'
	   << cp_quote(e.processing_code) << '\t'
	   << cp_quote(e.flow_code) << '\t'
	   << cp_quote(e.flags) << '\t'
	   << cp_quote(e.requirements) << '\t'
	   << cp_quote(e.provisions) << '\n';
    }
    return sa.take_string();
}

void
ElementMap::collect_indexes(const RouterT *router, Vector<int> &indexes,
			    ErrorHandler *errh) const
{
    indexes.clear();
    HashMap<String, int> primitives(-1);
    router->collect_primitive_classes(primitives);
    for (HashMap<String, int>::Iterator i = primitives.first(); i; i++)
	if (i.value() > 0) {
	    int t = elt_index(i.key());
	    if (t > 0)
		indexes.push_back(t);
	    else if (errh)
		errh->error("unknown element class `%s'", String(i.key()).cc());
	}
}

int
ElementMap::check_completeness(const RouterT *r, ErrorHandler *errh) const
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    int before = errh->nerrors();
    Vector<int> indexes;
    collect_indexes(r, indexes, errh);
    return (errh->nerrors() == before ? 0 : -1);
}

bool
ElementMap::driver_indifferent(const RouterT *r, int driver_mask, ErrorHandler *errh) const
{
    Vector<int> indexes;
    collect_indexes(r, indexes, errh);
    for (int i = 0; i < indexes.size(); i++) {
	int idx = indexes[i];
	if (idx > 0 && (_e[idx].driver_mask & driver_mask) != driver_mask)
	    return false;
    }
    return true;
}

bool
ElementMap::driver_compatible(const RouterT *router, int driver, ErrorHandler *errh = 0) const
{
    Vector<int> indexes;
    collect_indexes(router, indexes, errh);
    int mask = 1 << driver;
    for (int i = 0; i < indexes.size(); i++) {
	int idx = indexes[i];
	if (idx > 0 && !(_e[idx].driver_mask & mask)) {
	    while (idx > 0) {
		if (_e[idx].driver_mask & mask)
		    goto found;
		idx = _e[idx].uid_next;
	    }
	    return false;
	}
      found: ;
    }
    return true;
}

void
ElementMap::limit_driver(int driver)
{
    int mask = 1 << driver;
    for (HashMap<int, int>::Iterator i = _uid_map.first(); i; i++) {
	int t = i.value();
	while (t > 0 && !(_e[t].driver_mask & mask))
	    t = i.value() = _e[t].uid_next;
    }
}


bool
ElementMap::parse_default_file(const String &default_path, ErrorHandler *errh)
{
    String default_fn = clickpath_find_file("elementmap", "share/click", default_path);
    if (default_fn) {
	String text = file_string(default_fn, errh);
	parse(text);
	return true;
    } else {
	errh->warning("cannot find default elementmap");
	return false;
    }
}

bool
ElementMap::parse_requirement_files(RouterT *r, const String &default_path, ErrorHandler *errh, String *not_found_store = 0)
{
    String not_found;

    // try elementmap in archive
    int defaultmap_aei = r->archive_index("elementmap");
    if (defaultmap_aei >= 0)
	parse(r->archive(defaultmap_aei).data, "<archive>");

    // parse elementmaps for requirements in required order
    const Vector<String> &requirements = r->requirements();
    for (int i = 0; i < requirements.size(); i++) {
	String req = requirements[i];
	String mapname = "elementmap." + req;

	// look for elementmap in archive
	int map_aei = r->archive_index(mapname);
	if (map_aei >= 0)
	    parse(r->archive(map_aei).data, req);
	else {
	    String fn = clickpath_find_file(mapname, "share/click", default_path);
	    if (fn) {
		String text = file_string(fn, errh);
		parse(text, req);
	    } else {
		if (not_found)
		    not_found += ", ";
		not_found += "`" + req + "'";
	    }
	}
    }

    if (not_found_store)
	*not_found_store = not_found;
    if (not_found) {
	errh->warning("cannot find package-specific elementmaps:\n  %s", not_found.cc());
	return false;
    } else
	return true;
}

bool
ElementMap::parse_all_files(RouterT *r, const String &default_path, ErrorHandler *errh)
{
    bool found_default = parse_default_file(default_path, errh);
    bool found_other = parse_requirement_files(r, default_path, errh);

    if (found_default && found_other)
	return true;
    else {
	report_file_not_found(default_path, found_default, errh);
	return false;
    }
}

void
ElementMap::report_file_not_found(String default_path, bool found_default,
				  ErrorHandler *errh)
{
    if (!found_default)
	errh->message("(You may get unknown element class errors.\nTry `make install' or set the CLICKPATH evironment variable.");
    else
	errh->message("(You may get unknown element class errors.");

    const char *path = clickpath();
    bool allows_default = path_allows_default_path(path);
    if (!allows_default)
	errh->message("Searched in CLICKPATH `%s'.)", path);
    else if (!path)
	errh->message("Searched in install directory `%s'.)", default_path.cc());
    else
	errh->message("Searched in CLICKPATH and `%s'.)", default_path.cc());
}


// template instance
#include <click/vector.cc>
template class Vector<ElementMap::Elt>;

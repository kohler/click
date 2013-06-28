// -*- c-basic-offset: 4 -*-
/*
 * elementmap.{cc,hh} -- an element map class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2008-2009 Meraki, Inc.
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

int32_t ElementMap::version_counter = 0;
static ElementMap main_element_map;
ElementMap *ElementMap::the_element_map = &main_element_map;
static Vector<ElementMap *> element_map_stack;

ElementMap::ElementMap()
    : _name_map(0), _use_count(0), _driver_mask(Driver::ALLMASK),
      _provided_driver_mask(0)
{
    _e.push_back(Traits());
    _def.push_back(Globals());
    incr_version();
}

ElementMap::ElementMap(const String& str, ErrorHandler* errh)
    : _name_map(0), _use_count(0), _driver_mask(Driver::ALLMASK),
      _provided_driver_mask(0)
{
    _e.push_back(Traits());
    _def.push_back(Globals());
    parse(str, errh);
    incr_version();
}

ElementMap::~ElementMap()
{
    assert(_use_count == 0);
}


void
ElementMap::push_default(ElementMap *emap)
{
    emap->use();
    element_map_stack.push_back(the_element_map);
    the_element_map = emap;
}

void
ElementMap::pop_default()
{
    ElementMap *old = the_element_map;
    if (element_map_stack.size()) {
	the_element_map = element_map_stack.back();
	element_map_stack.pop_back();
    } else
	the_element_map = &main_element_map;
    old->unuse();
}

void
ElementMap::pop_default(ElementMap *emap)
{
    if (the_element_map == emap)
	pop_default();
}


int
ElementMap::driver_elt_index(int i) const
{
    while (i > 0 && (_e[i].driver_mask & _driver_mask) == 0)
	i = _e[i].name_next;
    return i;
}

String
ElementMap::documentation_url(const ElementTraits &t) const
{
    String name = t.documentation_name;
    if (name)
	return percent_substitute(_def[t.def_index].dochref,
				  's', name.c_str(),
				  0);
    else
	return "";
}

int
ElementMap::add(const Traits &e)
{
    int i = _e.size();
    _e.push_back(e);

    Traits &my_e = _e.back();
    if (my_e.requirements)
	my_e.calculate_driver_mask();

    if (e.name) {
	ElementClassT *c = ElementClassT::base_type(e.name);
	my_e.name_next = _name_map[c->name()];
	_name_map.set(c->name(), i);
    }

    incr_version();
    return i;
}

void
ElementMap::remove_at(int i)
{
    // XXX repeated removes can fill up ElementMap with crap
    if (i <= 0 || i >= _e.size())
	return;

    Traits &e = _e[i];
    int p = -1;
    for (int t = _name_map.get(e.name); t > 0; p = t, t = _e[t].name_next)
	/* nada */;
    if (p >= 0)
	_e[p].name_next = e.name_next;
    else if (e.name)
	_name_map.set(e.name, e.name_next);

    e.name = e.cxx = String();
    incr_version();
}

Traits &
ElementMap::force_traits(const String &class_name)
{
    int initial_i = _name_map[class_name], i = initial_i;
    if (!(_e[i].driver_mask & _driver_mask) && i > 0)
	i = driver_elt_index(i);
    if (i == 0) {
	Traits t;
	if (initial_i == 0)
	    t.name = class_name;
	else
	    t = _e[initial_i];
	t.driver_mask = _driver_mask;
	i = add(t);
    }
    return _e[i];
}

static const char *
parse_attribute_value(String *result,
		      const char *s, const char *ends,
		      const HashTable<String, String> &entities,
		      ErrorHandler *errh)
{
    while (s < ends && isspace((unsigned char) *s))
	s++;
    if (s >= ends || (*s != '\'' && *s != '\"')) {
	errh->error("XML parse error: missing attribute value");
	return s;
    }

    char quote = *s;
    const char *first = s + 1;
    StringAccum sa;
    for (s++; s < ends && *s != quote; s++)
	if (*s == '&') {
	    // dump on normal text
	    sa.append(first, s - first);

	    if (s + 3 < ends && s[1] == '#' && s[2] == 'x') {
		// hex character reference
		int c = 0;
		for (s += 3; isxdigit((unsigned char) *s); s++)
		    if (isdigit((unsigned char) *s))
			c = (c * 16) + *s - '0';
		    else
			c = (c * 16) + tolower((unsigned char) *s) - 'a' + 10;
		sa << (char)c;
	    } else if (s + 2 < ends && s[1] == '#') {
		// decimal character reference
		int c = 0;
		for (s += 2; isdigit((unsigned char) *s); s++)
		    c = (c * 10) + *s - '0';
		sa << (char)c;
	    } else {
		// named entity
		const char *t;
		for (t = s + 1; t < ends && *t != quote && *t != ';'; t++)
		    /* nada */;
		if (t < ends && *t == ';') {
		    String entity_name(s + 1, t - s - 1);
		    sa << entities[entity_name];
		    s = t;
		}
	    }

	    // check entity ended correctly
	    if (s >= ends || *s != ';') {
		errh->error("XML parse error: bad entity name");
		return s;
	    }

	    first = s + 1;
	}
    sa.append(first, s - first);
    if (s >= ends)
	errh->error("XML parse error: unterminated attribute value");
    else
	s++;
    *result = sa.take_string();
    return s;
}

static const char *
parse_xml_attrs(HashTable<String, String> &attrs,
		const char *s, const char *ends, bool *closed,
		const HashTable<String, String> &entities,
		ErrorHandler *errh)
{
    while (s < ends) {
	while (s < ends && isspace((unsigned char) *s))
	    s++;

	if (s >= ends)
	    return s;
	else if (*s == '/') {
	    *closed = true;
	    return s;
	} else if (*s == '>')
	    return s;

	// get attribute name
	const char *attrstart = s;
	while (s < ends && !isspace((unsigned char) *s) && *s != '=')
	    s++;
	if (s == attrstart) {
	    errh->error("XML parse error: missing attribute name");
	    return s;
	}
	String attrname(attrstart, s - attrstart);

	// skip whitespace and equals sign
	while (s < ends && isspace((unsigned char) *s))
	    s++;
	if (s >= ends || *s != '=') {
	    errh->error("XML parse error: missing %<=%>");
	    return s;
	}
	s++;

	// get attribute value
	String attrvalue;
	s = parse_attribute_value(&attrvalue, s, ends, entities, errh);
	attrs.set(attrname, attrvalue);
    }
    return s;
}

void
ElementMap::parse_xml(const String &str, const String &package_name, ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    // prepare entities
    HashTable<String, String> entities;
    entities.set("lt", "<");
    entities.set("amp", "&");
    entities.set("gt", ">");
    entities.set("quot", "\"");
    entities.set("apos", "'");

    const char *s = str.data();
    const char *ends = s + str.length();
    bool in_elementmap = false;

    while (s < ends) {
	// skip to '<'
	while (s < ends && *s != '<')
	    s++;
	for (s++; s < ends && isspace((unsigned char) *s); s++)
	    /* nada */;
	bool closed = false;
	if (s < ends && *s == '/') {
	    closed = true;
	    for (s++; s < ends && isspace((unsigned char) *s); s++)
		/* nada */;
	}

	// which tag
	if (s + 10 < ends && memcmp(s, "elementmap", 10) == 0
	    && (isspace((unsigned char) s[10]) || s[10] == '>' || s[10] == '/')) {
	    // parse elementmap tag
	    if (!closed) {
		if (in_elementmap)
		    errh->error("XML elementmap parse error: nested <elementmap> tags");
		HashTable<String, String> attrs;
		s = parse_xml_attrs(attrs, s + 10, ends, &closed, entities, errh);
		Globals g;
		g.package = (attrs["package"] ? attrs["package"] : package_name);
		g.srcdir = attrs["sourcedir"];
		if (attrs["src"].substring(0, 7) == "file://")
		    g.srcdir = attrs["src"].substring(7);
		g.dochref = attrs["dochref"];
		if (!g.dochref)
		    g.dochref = attrs["webdoc"];
		if (attrs["provides"])
		    _e[0].provisions += " " + attrs["provides"];
		g.driver_mask = Driver::ALLMASK;
		if (attrs["drivers"])
		    g.driver_mask = Driver::driver_mask(attrs["drivers"]);
		if (!_provided_driver_mask)
		    _provided_driver_mask = g.driver_mask;
		_def.push_back(g);
		in_elementmap = true;
	    }
	    if (closed)
		in_elementmap = false;

	} else if (s + 5 < ends && memcmp(s, "entry", 5) == 0
		   && (isspace((unsigned char) s[5]) || s[5] == '>' || s[5] == '/')
		   && !closed && in_elementmap) {
	    // parse entry tag
	    HashTable<String, String> attrs;
	    s = parse_xml_attrs(attrs, s + 5, ends, &closed, entities, errh);
	    Traits elt;
	    for (HashTable<String, String>::iterator i = attrs.begin(); i.live(); i++)
		if (String *sp = elt.component(i.key()))
		    *sp = i.value();
	    if (elt.provisions || elt.name) {
		elt.def_index = _def.size() - 1;
		(void) add(elt);
	    }

	} else if (s + 7 < ends && memcmp(s, "!ENTITY", 7) == 0
		 && (isspace((unsigned char) s[7]) || s[7] == '>' || s[7] == '/')) {
	    // parse entity declaration
	    for (s += 7; s < ends && isspace((unsigned char) *s); s++)
		/* nada */;
	    if (s >= ends || *s == '%') // skip DTD entities
		break;
	    const char *name_start = s;
	    while (s < ends && !isspace((unsigned char) *s))
		s++;
	    String name(name_start, s - name_start), value;
	    s = parse_attribute_value(&value, s, ends, entities, errh);
	    entities.set(name, value);

	} else if (s + 8 < ends && memcmp(s, "![CDATA[", 8) == 0) {
	    // skip CDATA section
	    for (s += 8; s < ends; s++)
		if (*s == ']' && s + 3 <= ends && memcmp(s, "]]>", 3) == 0)
		    break;
	} else if (s + 3 < ends && memcmp(s, "!--", 3) == 0) {
	    // skip comment
	    for (s += 3; s < ends; s++)
		if (*s == '-' && s + 3 <= ends && memcmp(s, "-->", 3) == 0)
		    break;
	}

	// skip to '>'
	while (s < ends && *s != '>')
	    s++;
    }
}

void
ElementMap::parse(const String &str, const String &package_name, ErrorHandler *errh)
{
    if (str.length() && str[0] == '<') {
	parse_xml(str, package_name, errh);
	return;
    }

    int def_index = 0;
    if (package_name != _def[0].package) {
	def_index = _def.size();
	_def.push_back(Globals());
	_def.back().package = package_name;
    }

    // set up default data
    Vector<int> data;
    for (int i = Traits::D_FIRST_DEFAULT; i <= Traits::D_LAST_DEFAULT; i++)
	data.push_back(i);

    // loop over the lines
    const char *begin = str.begin();
    const char *end = str.end();
    while (begin < end) {
	// read a line
	String line = str.substring(begin, find(begin, end, '\n'));
	begin = line.end() + 1;

	// break into words
	Vector<String> words;
	cp_spacevec(line, words);

	// skip blank lines & comments
	if (words.size() == 0 || words[0][0] == '#')
	    continue;

	// check for $sourcedir
	if (words[0] == "$sourcedir") {
	    if (words.size() == 2) {
		def_index = _def.size();
		_def.push_back(Globals());
		_def.back() = _def[def_index - 1];
		_def.back().srcdir = cp_unquote(words[1]);
	    }

	} else if (words[0] == "$webdoc") {
	    if (words.size() == 2) {
		def_index = _def.size();
		_def.push_back(Globals());
		_def.back() = _def[def_index - 1];
		_def.back().dochref = cp_unquote(words[1]);
	    }

	} else if (words[0] == "$provides") {
	    for (int i = 1; i < words.size(); i++)
		_e[0].provisions += " " + cp_unquote(words[i]);

	} else if (words[0] == "$data") {
	    data.clear();
	    for (int i = 1; i < words.size(); i++)
		data.push_back(Traits::parse_component(cp_unquote(words[i])));

	} else if (words[0][0] != '$') {
	    // an actual line
	    Traits elt;
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
ElementMap::parse(const String &str, ErrorHandler *errh)
{
    parse(str, String(), errh);
}

String
ElementMap::unparse(const String &package) const
{
    StringAccum sa;
    sa << "<?xml version=\"1.0\" standalone=\"yes\"?>\n\
<elementmap xmlns=\"http://www.lcdf.org/click/xml/\"";
    if (package)
	sa << " package=\"" << xml_quote(package) << "\"";
    sa << ">\n";
    for (int i = 1; i < _e.size(); i++) {
	const Traits &e = _e[i];
	if (!e.name && !e.cxx)
	    continue;
	sa << "  <entry";
	if (e.name)
	    sa << " name=\"" << xml_quote(e.name) << "\"";
	if (e.cxx)
	    sa << " cxxclass=\"" << xml_quote(e.cxx) << "\"";
	if (e.documentation_name)
	    sa << " docname=\"" << xml_quote(e.documentation_name) << "\"";
	if (e.header_file)
	    sa << " headerfile=\"" << xml_quote(e.header_file) << "\"";
	if (e.source_file)
	    sa << " sourcefile=\"" << xml_quote(e.source_file) << "\"";
	sa << " processing=\"" << xml_quote(e.processing_code)
	   << "\" flowcode=\"" << xml_quote(e.flow_code) << "\"";
	if (e.flags)
	    sa << " flags=\"" << xml_quote(e.flags) << "\"";
	if (e.requirements)
	    sa << " requires=\"" << xml_quote(e.requirements) << "\"";
	if (e.provisions)
	    sa << " provides=\"" << xml_quote(e.provisions) << "\"";
	if (e.noexport)
	    sa << " noexport=\"" << xml_quote(e.noexport) << "\"";
	sa << " />\n";
    }
    sa << "</elementmap>\n";
    return sa.take_string();
}

String
ElementMap::unparse_nonxml() const
{
    StringAccum sa;
    sa << "$data\tname\tcxxclass\tdocname\theaderfile\tprocessing\tflowcode\tflags\trequires\tprovides\n";
    for (int i = 1; i < _e.size(); i++) {
	const Traits &e = _e[i];
	if (!e.name && !e.cxx)
	    continue;
	sa << cp_quote(e.name) << '\t'
	   << cp_quote(e.cxx) << '\t'
	   << cp_quote(e.documentation_name) << '\t'
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
    HashTable<ElementClassT *, int> types(-1);
    router->collect_types(types);
    for (HashTable<ElementClassT *, int>::iterator i = types.begin(); i.live(); i++)
	if (i.key()->primitive()) {
	    int t = _name_map[i.key()->name()];
	    if (t > 0)
		indexes.push_back(t);
	    else if (errh)
		errh->error("unknown element class %<%s%>", i.key()->printable_name_c_str());
	}
}

int
ElementMap::check_completeness(const RouterT *r, ErrorHandler *errh) const
{
    LocalErrorHandler lerrh(errh);
    Vector<int> indexes;
    collect_indexes(r, indexes, &lerrh);
    return (lerrh.nerrors() ? -1 : 0);
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

int
ElementMap::compatible_driver_mask(const RouterT *r, ErrorHandler *errh) const
{
    Vector<int> indexes;
    collect_indexes(r, indexes, errh);
    int mask = Driver::ALLMASK;
    for (int i = 0; i < indexes.size(); i++) {
	int idx = indexes[i];
	int elt_mask = 0;
	while (idx > 0) {
	    int idx_mask = _e[idx].driver_mask;
	    if (idx_mask & (1 << Driver::MULTITHREAD))
		for (int d = 0; d < Driver::COUNT; ++d)
		    if ((idx_mask & (1 << d))
			&& !provides_global(Driver::multithread_name(d)))
			idx_mask &= ~(1 << d);
	    elt_mask |= idx_mask;
	    idx = _e[idx].name_next;
	}
	mask &= elt_mask;
    }
    return mask;
}

bool
ElementMap::driver_compatible(const RouterT *r, int driver, ErrorHandler *errh) const
{
    return compatible_driver_mask(r, errh) & (1 << driver);
}

void
ElementMap::set_driver_mask(int driver_mask)
{
    if (_driver_mask != driver_mask)
	incr_version();
    _driver_mask = driver_mask;
}

int
ElementMap::pick_driver(int wanted_driver, const RouterT* router, ErrorHandler* errh) const
{
    LocalErrorHandler lerrh(errh);
    int driver_mask = compatible_driver_mask(router, &lerrh);
    if (driver_mask == 0) {
	lerrh.warning("configuration not compatible with any driver");
	return Driver::USERLEVEL;
    }
    if ((driver_mask & _provided_driver_mask) == 0) {
	lerrh.warning("configuration not compatible with installed drivers");
	driver_mask = _provided_driver_mask;
    }
    if (wanted_driver >= 0) {
	if (!(driver_mask & (1 << wanted_driver)))
	    lerrh.warning("configuration not compatible with %s driver", Driver::name(wanted_driver));
	return wanted_driver;
    }

    for (int d = Driver::COUNT - 1; d >= 0; d--)
	if (driver_mask & (1 << d))
	    wanted_driver = d;
    // don't complain if only a single driver works
    if ((driver_mask & (driver_mask - 1)) != 0
	&& !driver_indifferent(router, driver_mask, errh))
	lerrh.warning("configuration not indifferent to driver, picking %s\n(You might want to specify a driver explicitly.)", Driver::name(wanted_driver));
    return wanted_driver;
}


bool
ElementMap::parse_default_file(const String &default_path, ErrorHandler *errh)
{
    String default_fn = clickpath_find_file("elementmap.xml", "share", default_path);
    if (!default_fn)
	default_fn = clickpath_find_file("elementmap", "share", default_path);
    if (default_fn) {
	String text = file_string(default_fn, errh);
	parse(text);
	return true;
    } else {
	errh->warning("default elementmap missing");
	return false;
    }
}

bool
ElementMap::parse_package_file(const String& package_name, const RouterT* r, const String& default_path, ErrorHandler* errh)
{
    String mapname = "elementmap-" + package_name + ".xml";
    String mapname2 = "elementmap." + package_name;

    // look for elementmap in archive
    if (r) {
        int map_aei = r->archive_index(mapname);
        if (map_aei < 0)
            map_aei = r->archive_index(mapname2);
        if (map_aei >= 0) {
            parse(r->archive(map_aei).data, package_name);
            return true;
        }
    }

    // look for elementmap in file system
    String fn = clickpath_find_file(mapname, "share", default_path);
    if (!fn)
        fn = clickpath_find_file(mapname2, "share", default_path);
    if (fn) {
        String text = file_string(fn, errh);
        parse(text, package_name);
        return true;
    } else {
        if (errh)
            errh->warning("elementmap for package %<%s%> missing", package_name.c_str());
        return false;
    }
}

bool
ElementMap::parse_requirement_files(RouterT *r, const String &default_path, ErrorHandler *errh, String *not_found_store)
{
    String not_found;

    // try elementmap in archive
    int defaultmap_aei = r->archive_index("elementmap.xml");
    if (defaultmap_aei < 0)
	defaultmap_aei = r->archive_index("elementmap");
    if (defaultmap_aei >= 0)
	parse(r->archive(defaultmap_aei).data, "<archive>");

    // parse elementmaps for requirements in required order
    const Vector<String> &requirements = r->requirements();
    for (int i = 0; i < requirements.size(); i += 2) {
	if (!requirements[i].equals("package", 7))
	    continue;
        if (!parse_package_file(requirements[i+1], r, default_path, 0)) {
	    if (not_found)
		not_found += ", ";
	    not_found += "'" + requirements[i+1] + "'";
	}
    }

    if (not_found_store)
	*not_found_store = not_found;
    if (not_found) {
	errh->warning("package elementmaps missing:\n  %s", not_found.c_str());
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
	errh->message("(The %<elementmap.xml%> file stores information about available elements,\nand is required for correct operation.  %<make install%> should install it.");
    else
	errh->message("(You may get unknown element class errors.");

    const char *path = clickpath();
    bool allows_default = path_allows_default_path(path);
    if (!allows_default)
	errh->message("Searched in CLICKPATH %<%s%>.)", path);
    else if (!path)
	errh->message("Searched in install directory %<%s%>.)", default_path.c_str());
    else
	errh->message("Searched in CLICKPATH and %<%s%>.)", default_path.c_str());
}


// TraitsIterator

ElementMap::TraitsIterator::TraitsIterator(const ElementMap *emap, bool elements_only)
    : _emap(emap), _index(0), _elements_only(elements_only)
{
    (*this)++;
}

void
ElementMap::TraitsIterator::operator++(int)
{
    _index++;
    while (_index < _emap->size()) {
	const ElementTraits &t = _emap->traits_at(_index);
	if ((t.driver_mask & _emap->driver_mask())
	    && (t.name || t.cxx)
	    && (t.name || !_elements_only))
	    break;
	_index++;
    }
}

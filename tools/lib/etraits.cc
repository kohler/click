// -*- c-basic-offset: 4 -*-
/*
 * etraits.{cc,hh} -- class records element traits
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
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
#include "etraits.hh"
#include "toolutils.hh"
#include <click/confparse.hh>

ElementTraits ElementTraits::the_null_traits;

static const char * const driver_names[] = {
    "userlevel", "linuxmodule", "bsdmodule", "ns", "multithread"
};

static const char * const driver_multithread_names[] = {
    "umultithread", "smpclick", "??", "??"
};

const char *
Driver::name(int d)
{
    static_assert(USERLEVEL == 0 && LINUXMODULE == 1 && BSDMODULE == 2 && NSMODULE == 3, "Constant misassignments.");
    if (d >= 0 && d <= COUNT)
	return driver_names[d];
    else
	return "??";
}

const char *
Driver::multithread_name(int d)
{
    if (d >= 0 && d < COUNT)
	return driver_multithread_names[d];
    else
	return "??";
}

const char *
Driver::requirement(int d)
{
    if (d >= 0 && d <= COUNT)
	return driver_names[d];
    else
	return "";
}

int
Driver::driver(const String& name)
{
    for (int d = 0; d < COUNT; d++)
	if (name == driver_names[d])
	    return d;
    return -1;
}

int
Driver::driver_mask(const String& name)
{
    int d = driver(name);
    if (d >= 0)
	return 1 << d;

    int m = 0;
    const char* s = name.begin(), *end = name.end();
    while (s != end) {
	const char *word = s;
	while (s != end && *s != '|' && !isspace((unsigned char) *s))
	    ++s;
	if ((d = driver(name.substring(word, s))) >= 0)
	    m |= 1 << d;
	if (s != end)
	    ++s;
    }
    return m;
}


static bool
requirement_contains(const String &req, const String &n)
{
    assert(n.length());
    int pos = 0;
    while ((pos = req.find_left(n, pos)) >= 0) {
	int rpos = pos + n.length();
	// XXX should be more careful about '|' bars
	if ((pos == 0 || isspace((unsigned char) req[pos - 1]) || req[pos - 1] == '|')
	    && (rpos == req.length() || isspace((unsigned char) req[rpos]) || req[rpos] == '|'))
	    return true;
	pos = rpos;
    }
    return false;
}

bool
ElementTraits::requires(const String &n) const
{
    if (!requirements)
	return false;
    else
	return requirement_contains(requirements, n);
}

bool
ElementTraits::provides(const String &n) const
{
    if (n == name)
	return true;
    else if (!provisions)
	return false;
    else
	return requirement_contains(provisions, n);
}

int
ElementTraits::hard_flag_value(const String &str) const
{
    assert(str);
    const char *s = flags.begin(), *end = flags.end() - str.length();
    while (s <= end) {
	if (memcmp(s, str.begin(), str.length()) == 0
	    && (s == end || isdigit((unsigned char) s[str.length()])
		|| isspace((unsigned char) s[str.length()])
		|| s[str.length()] == '=')) {
	    s += str.length();
	    end = flags.end();
	    if (s != end && *s == '=')
		++s;
	    if (s == end || !isdigit((unsigned char) *s))
		return 1;
	    int i = 0;
	    do {
		i = i * 10 + *s - '0';
		++s;
	    } while (s != end && isdigit((unsigned char) *s));
	    return (s == end || isspace((unsigned char) *s) ? i : 1);
	} else
	    while (s <= end && !isspace((unsigned char) *s))
		++s;
    }
    return -1;
}

void
ElementTraits::calculate_driver_mask()
{
    driver_mask = 0;
    if (requirement_contains(requirements, "userlevel"))
	driver_mask |= 1 << Driver::USERLEVEL;
    if (requirement_contains(requirements, "linuxmodule"))
	driver_mask |= 1 << Driver::LINUXMODULE;
    if (requirement_contains(requirements, "bsdmodule"))
	driver_mask |= 1 << Driver::BSDMODULE;
    if (requirement_contains(requirements, "ns"))
	driver_mask |= 1 << Driver::NSMODULE;
    if (driver_mask == 0)
	driver_mask = Driver::ALLMASK;
    if (requirement_contains(requirements, "multithread"))
	driver_mask |= 1 << Driver::MULTITHREAD;
}

String *
ElementTraits::component(int what)
{
    switch (what) {
      case D_CLASS:		return &name;
      case D_CXX_CLASS:		return &cxx;
      case D_HEADER_FILE:	return &header_file;
      case D_SOURCE_FILE:	return &source_file;
      case D_PORT_COUNT:	return &port_count_code;
      case D_PROCESSING:	return &processing_code;
      case D_FLOW_CODE:		return &flow_code;
      case D_FLAGS:		return &flags;
      case D_METHODS:		return &methods;
      case D_REQUIREMENTS:	return &requirements;
      case D_PROVISIONS:	return &provisions;
      case D_LIBS:		return &libs;
      case D_DOC_NAME:		return &documentation_name;
      case D_NOEXPORT:		return &noexport;
      default:			return 0;
    }
}

static HashTable<String, int> components(ElementTraits::D_NONE);
static bool components_initialized = false;

int
ElementTraits::parse_component(const String &s)
{
    if (!components_initialized) {
	components.set("name", D_CLASS);
	components.set("cxxclass", D_CXX_CLASS);
	components.set("headerfile", D_HEADER_FILE);
	components.set("sourcefile", D_SOURCE_FILE);
	components.set("portcount", D_PORT_COUNT);
	components.set("processing", D_PROCESSING);
	components.set("flowcode", D_FLOW_CODE);
	components.set("methods", D_METHODS);
	components.set("requires", D_REQUIREMENTS);
	components.set("provides", D_PROVISIONS);
	components.set("libs", D_LIBS);
	components.set("docname", D_DOC_NAME);
	components.set("flags", D_FLAGS);
	components.set("noexport", D_NOEXPORT);
	// for compatibility
	components.set("class", D_CLASS);
	components.set("cxx_class", D_CXX_CLASS);
	components.set("header_file", D_HEADER_FILE);
	components.set("source_file", D_SOURCE_FILE);
	components.set("requirements", D_REQUIREMENTS);
	components.set("provisions", D_PROVISIONS);
	components.set("doc_name", D_DOC_NAME);
    }

    return components.get(s);
}

ElementTraits
ElementTraits::make(int component, ...)
{
    va_list val;
    va_start(val, component);
    ElementTraits t;
    while (component != D_NONE) {
	const char *x = va_arg(val, const char *);
	if (String *c = t.component(component))
	    *c = x;
	component = va_arg(val, int);
    }
    va_end(val);
    return t;
}

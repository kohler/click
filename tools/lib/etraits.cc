// -*- c-basic-offset: 4 -*-
/*
 * etraits.{cc,hh} -- class records element traits
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
#include "etraits.hh"
#include "toolutils.hh"
#include <click/confparse.hh>

static String::Initializer string_initializer;
ElementTraits ElementTraits::the_null_traits;

const char *
Driver::name(int d)
{
    if (d == USERLEVEL)
	return "userlevel";
    else if (d == LINUXMODULE)
	return "linuxmodule";
    else if (d == BSDMODULE)
	return "bsdmodule";
    else if (d == NSMODULE)
	return "ns";
    else
	return "??";
}

const char *
Driver::requirement(int d)
{
    if (d == USERLEVEL)
	return "userlevel";
    else if (d == LINUXMODULE)
	return "linuxmodule";
    else if (d == BSDMODULE)
	return "bsdmodule";
    else if (d == NSMODULE)
	return "ns";
    else
	return "";
}

static bool
requirement_contains(const String &req, const String &n)
{
    assert(n.length());
    int pos = 0;
    while ((pos = req.find_left(n, pos)) >= 0) {
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
ElementTraits::flag_value(int flag) const
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
}

String *
ElementTraits::component(int what)
{
    switch (what) {
      case D_CLASS:		return &name;
      case D_CXX_CLASS:		return &cxx;
      case D_HEADER_FILE:	return &header_file;
      case D_SOURCE_FILE:	return &source_file;
      case D_PROCESSING:	return &_processing_code;
      case D_FLOW_CODE:		return &_flow_code;
      case D_FLAGS:		return &flags;
      case D_REQUIREMENTS:	return &requirements;
      case D_PROVISIONS:	return &provisions;
      case D_DOC_NAME:		return &documentation_name;
      default:			return 0;
    }
}

int
ElementTraits::parse_component(const String &s)
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
    else if (s == "doc_name")
	return D_DOC_NAME;
    else
	return D_NONE;
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

// template instance
#include <click/vector.cc>
template class Vector<ElementTraits>;

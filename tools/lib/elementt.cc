// -*- c-basic-offset: 4 -*-
/*
 * elementt.{cc,hh} -- tool definition of element
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

#include "elementt.hh"
#include "eclasst.hh"
#include "routert.hh"
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/variableenv.hh>
#include <stdlib.h>

ElementT::ElementT()
    : flags(0), _idx(-1), _type(0), _tunnel_input(0), _tunnel_output(0),
      _owner(0)
{
}

ElementT::ElementT(const String &n, ElementClassT *eclass,
		   const String &config, const String &lm)
    : flags(0), _idx(-1), _name(n),
      _type(eclass), _configuration(config), _landmark(lm),
      _ninputs(0), _noutputs(0), _tunnel_input(0), _tunnel_output(0),
      _owner(0)
{
    assert(_type);
    assert(name_ok(_name, true));
    _type->use();
}

ElementT::ElementT(const ElementT &o)
    : flags(o.flags), _idx(-1), _name(o._name),
      _type(o._type), _configuration(o._configuration), _landmark(o._landmark),
      _ninputs(0), _noutputs(0), _tunnel_input(0), _tunnel_output(0),
      _owner(0)
{
    if (_type)
	_type->use();
}

ElementT::~ElementT()
{
    if (_type)
	_type->unuse();
}

bool
ElementT::name_ok(const String &name, bool allow_anon_names)
{
    const char *data = name.data();
    int pos = 0, len = name.length();

    // check anonymous name syntax
    if (len > 0 && data[pos] == ';' && allow_anon_names) {
	pos++;
	int epos = len - 1;
	while (epos > 1 && isdigit(data[epos]))
	    epos--;
	if (epos == len - 1 || data[epos] != '@')
	    return false;
    }
    
    // must have at least one character, must not start with slash
    if (pos >= len || data[pos] == '/')
	return false;
    while (1) {
	if (isdigit(data[pos])) { // check for all-digit component
	    while (pos < len && isdigit(data[pos]))
		pos++;
	    if (pos >= len || data[pos] == '/')
		return false;
	}
	while (pos < len && (isalnum(data[pos]) || data[pos] == '_' || data[pos] == '@'))
	    pos++;
	if (pos == len)
	    return true;
	else if (data[pos] != '/' || pos == len - 1 || data[pos + 1] == '/')
	    return false;
	else
	    pos++;
    }
}

void
ElementT::redeclaration_error(ErrorHandler *errh, const char *what, String name, const String &landmark, const String &old_landmark)
{
    if (!what)
	what = "";
    const char *sp = (strlen(what) ? " " : "");
    errh->lerror(landmark, "redeclaration of %s%s`%s'", what, sp, name.cc());
    errh->lerror(old_landmark, "`%s' previously declared here", name.cc());
}


int
PortT::index_in(const Vector<PortT> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    return -1;
}

int
PortT::force_index_in(Vector<PortT> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    v.push_back(*this);
    return size;
}

extern "C" {
static int
PortT_sorter(const void *av, const void *bv)
{
    const PortT *a = (const PortT *)av, *b = (const PortT *)bv;
    if (a->elt == b->elt)
	return a->port - b->port;
    else
	return a->elt->idx() - b->elt->idx();
}
}

void
PortT::sort(Vector<PortT> &v)
{
    qsort(&v[0], v.size(), sizeof(PortT), PortT_sorter);
}

String
PortT::unparse_input() const
{
    if (elt)
	return "[" + String(port) + "]" + elt->name();
    else
	return "<>";
}

String
PortT::unparse_output() const
{
    if (elt)
	return elt->name() + "[" + String(port) + "]";
    else
	return "<>";
}


ConnectionT::ConnectionT()
    : _from(), _to(), _landmark(), _next_from(-1), _next_to(-1)
{
}

ConnectionT::ConnectionT(const PortT &from, const PortT &to, const String &lm)
    : _from(from), _to(to), _landmark(lm), _next_from(-1), _next_to(-1)
{
}

ConnectionT::ConnectionT(const PortT &from, const PortT &to, const String &lm, int next_from, int next_to)
    : _from(from), _to(to), _landmark(lm),
      _next_from(next_from), _next_to(next_to)
{
}

String
ConnectionT::unparse() const
{
    return _from.unparse_output() + " -> " + _to.unparse_input();
}

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


int
Hookup::index_in(const Vector<Hookup> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    return -1;
}

int
Hookup::force_index_in(Vector<Hookup> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    v.push_back(*this);
    return size;
}

int
Hookup::sorter(const void *av, const void *bv)
{
    const Hookup *a = (const Hookup *)av, *b = (const Hookup *)bv;
    if (a->elt == b->elt)
	return a->port - b->port;
    else
	return a->elt->idx() - b->elt->idx();
}

void
Hookup::sort(Vector<Hookup> &v)
{
    qsort(&v[0], v.size(), sizeof(Hookup), &sorter);
}


int
HookupI::index_in(const Vector<HookupI> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    return -1;
}

int
HookupI::force_index_in(Vector<HookupI> &v, int start) const
{
    int size = v.size();
    for (int i = start; i < size; i++)
	if (v[i] == *this)
	    return i;
    v.push_back(*this);
    return size;
}

int
HookupI::sorter(const void *av, const void *bv)
{
    const HookupI *a = (const HookupI *)av, *b = (const HookupI *)bv;
    if (a->idx == b->idx)
	return a->port - b->port;
    else
	return a->idx - b->idx;
}

void
HookupI::sort(Vector<HookupI> &v)
{
    qsort(&v[0], v.size(), sizeof(HookupI), &sorter);
}


ConnectionT::ConnectionT()
    : _from(), _to(), _landmark(), _next_from(-1), _next_to(-1)
{
}

ConnectionT::ConnectionT(const Hookup &from, const Hookup &to, const String &lm)
    : _from(from), _to(to), _landmark(lm), _next_from(-1), _next_to(-1)
{
}

ConnectionT::ConnectionT(const Hookup &from, const Hookup &to, const String &lm, int next_from, int next_to)
    : _from(from), _to(to), _landmark(lm),
      _next_from(next_from), _next_to(next_to)
{
}

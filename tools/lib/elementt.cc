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
    : flags(0), _type(0), _tunnel_input(-1), _tunnel_output(-1)
{
}

ElementT::ElementT(const String &n, ElementClassT *eclass,
		   const String &config, const String &lm)
    : name(n), flags(0),
      _type(eclass), _configuration(config), _landmark(lm),
      _ninputs(0), _noutputs(0), _tunnel_input(-1), _tunnel_output(-1)
{
    assert(_type);
    _type->use();
}

ElementT::ElementT(const ElementT &o)
    : name(o.name), flags(o.flags),
      _type(o._type), _configuration(o._configuration), _landmark(o._landmark),
      _ninputs(o._ninputs), _noutputs(o._noutputs),
      _tunnel_input(o._tunnel_input), _tunnel_output(o._tunnel_output)
{
    if (_type)
	_type->use();
}

ElementT::~ElementT()
{
    if (_type)
	_type->unuse();
}

ElementT &
ElementT::operator=(const ElementT &o)
{
    if (o._type)
	o._type->use();
    if (_type)
	_type->unuse();
    _type = o._type;
    name = o.name;
    _configuration = o._configuration;
    _landmark = o._landmark;
    _tunnel_input = -1;
    _tunnel_output = -1;
    _ninputs = 0;
    _noutputs = 0;
    flags = o.flags;
    return *this;
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
    if (a->idx == b->idx)
	return a->port - b->port;
    else
	return a->idx - b->idx;
}

void
Hookup::sort(Vector<Hookup> &v)
{
    qsort(&v[0], v.size(), sizeof(Hookup), &sorter);
}

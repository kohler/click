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
    : tunnel_input(-1), tunnel_output(-1), flags(0), _type(0)
{
}

ElementT::ElementT(const String &n, ElementClassT *eclass,
		   const String &config, const String &lm)
    : name(n), configuration(config),
      tunnel_input(-1), tunnel_output(-1), landmark(lm), flags(0), _type(eclass)
{
    assert(_type);
    _type->use();
}

ElementT::ElementT(const ElementT &e)
    : name(e.name), configuration(e.configuration),
      tunnel_input(e.tunnel_input), tunnel_output(e.tunnel_output),
      landmark(e.landmark), flags(e.flags), _type(e._type)
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
    configuration = o.configuration;
    tunnel_input = o.tunnel_input;
    tunnel_output = o.tunnel_output;
    landmark = o.landmark;
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

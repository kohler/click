// -*- c-basic-offset: 4 -*-
/*
 * radixiplookup.{cc,hh} -- looks up next-hop address in radix table
 * Thomer M. Gil, Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "radixiplookup.hh"

RadixIPLookup::RadixIPLookup()
    : _entries(0)
{
    MOD_INC_USE_COUNT;
    _radix = new Radix;
}

RadixIPLookup::~RadixIPLookup()
{
    MOD_DEC_USE_COUNT;
    uninitialize();
}


void
RadixIPLookup::notify_noutputs(int n)
{
    set_noutputs(n);
}

void
RadixIPLookup::uninitialize()
{
    for (int i = 0; i < _v.size(); i++)
	if (_v[i])
	    delete _v[i];
    _v.clear();
    delete _radix;
    _radix = 0;
}


String
RadixIPLookup::dump_routes()
{
    StringAccum sa;
    unsigned dst, mask, gw, port;

    sa << "Entries: " << _entries << "\nDST/MASK\tGW\tPORT\n";

    int seen = 0; // # of valid entries handled
    for (int i = 0; seen < _entries; i++)
	if (get(i, dst, mask, gw, port)) {
	    sa << IPAddress(dst) << '/' << IPAddress(mask) << '\t'
	       << IPAddress(gw) << '\t' << port << '\n';
	    seen++;
	}
    return sa.take_string();
}


int
RadixIPLookup::add_route(IPAddress d, IPAddress m, IPAddress g, int port, ErrorHandler *errh)
{
    unsigned dst = d.addr();
    unsigned mask = m.addr();
    unsigned gw = g.addr();
    dst &= mask;

    for (int i = 0; i < _v.size(); i++)
	if (_v[i]->valid && (_v[i]->dst == dst) && (_v[i]->mask == mask)) {
	    _v[i]->gw = gw;
	    _v[i]->port = port;
	    return 0;
	}

    if (Entry *e = new Entry) {
	e->dst = dst;
	e->mask = mask;
	e->gw = gw;
	e->port = port;
	e->valid = true;
	_v.push_back(e);
	_entries++;
	_radix->insert((dst & mask), _v.size() - 1);
	return 0;
    } else
	return errh->error("out of memory");
}

int
RadixIPLookup::remove_route(IPAddress d, IPAddress m, ErrorHandler *errh)
{
    unsigned dst = d.addr();
    unsigned mask = m.addr();

    for (int i = 0; i < _v.size(); i++)
	if (_v[i]->valid && (_v[i]->dst == dst) && (_v[i]->mask == mask)) {
	    _v[i]->valid = 0;
	    _entries--;
	    _radix->del(dst & mask);
	    return 0;
	}

    return errh->error("no such route");
}

int
RadixIPLookup::lookup_route(IPAddress d, IPAddress &gw) const
{
    unsigned dst = d.addr();
    int index;

    if (!_entries || !_radix->lookup(dst, index))
	goto nomatch;

    // consider this a match if dst is part of range described by routing table
    if((dst & _v[index]->mask) == (_v[index]->dst & _v[index]->mask)) {
	gw = _v[index]->gw;
	return _v[index]->port;
    }

  nomatch:
    gw = 0;
    return -1;
}

bool
RadixIPLookup::get(int i, unsigned &dst, unsigned &mask, unsigned &gw, unsigned &port)
{
    assert(i >= 0 && i < _v.size());
    if(i < 0 || i >= _v.size() || _v[i]->valid == 0) {
	dst = mask = gw = port = 0;
	return false;
    }
    dst = _v[i]->dst;
    mask = _v[i]->mask;
    gw = _v[i]->gw;
    port = _v[i]->port;
    return true;
}

// generate Vector template instance
#include <click/vector.cc>
template class Vector<RadixIPLookup::Entry>;

ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(RadixIPLookup)

// -*- c-basic-offset: 4 -*-
/*
 * radixiplookup.{cc,hh} -- looks up next-hop address in radix table
 * Thomer M. Gil, Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2005 Regents of the University of California
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
CLICK_DECLS

RadixIPLookup::Radix*
RadixIPLookup::Radix::make_radix(int bitshift, int n)
{
    if (Radix* r = (Radix*) new unsigned char[sizeof(Radix) + n * sizeof(Child)]) {
	r->_route_index = 0x7FFFFFFF;
	r->_bitshift = bitshift;
	r->_n = n;
	r->_nchildren = 0;
	memset(r->_children, 0, n * sizeof(Child));
	for (int i = 0; i < n; i++)
	    r->_children[i].key = -1;
	return r;
    } else
	return 0;
}

RadixIPLookup::Radix*
RadixIPLookup::Radix::change(uint32_t addr, uint32_t naddr, int key, uint32_t key_priority)
{
    if (naddr == 0) {
	for (int i = 0; i < _n; i++)
	    if (_children[i].key_priority <= key_priority) {
		_children[i].key = key;
		_children[i].key_priority = (key < 0 ? 0 : key_priority);
	    }
    } else {
	int i1 = addr >> _bitshift;
	int i2 = i1 + (naddr >> _bitshift);
	addr &= (1 << _bitshift) - 1;
	naddr &= (1 << _bitshift) - 1;

	if (i1 == i2) {
	    if (!_children[i1].child) {
		if ((_children[i1].child = make_radix(_bitshift - 4, 16)))
		    _nchildren++;
	    }
	    if (_children[i1].child)
		return _children[i1].child->change(addr, naddr, key, key_priority);
	}
	
	while (i1 < i2) {
	    if (_children[i1].key_priority <= key_priority) {
		if ((_children[i1].key >= 0) != (key >= 0))
		    _nchildren += (key >= 0 ? 1 : -1);
		_children[i1].key = key;
		_children[i1].key_priority = (key < 0 ? 0 : key_priority);
	    }
	    i1++;
	}
    }
    
    return this;
}


RadixIPLookup::RadixIPLookup()
    : _default_key(-1), _radix(Radix::make_radix(24, 256))
{
    add_input();
}

RadixIPLookup::~RadixIPLookup()
{
}


void
RadixIPLookup::notify_noutputs(int n)
{
    set_noutputs(n);
}

void
RadixIPLookup::cleanup(CleanupStage)
{
    _v.clear();
    Radix::free_radix(_radix);
    _radix = 0;
}


String
RadixIPLookup::dump_routes() const
{
    StringAccum sa;
    for (int i = 0; i < _v.size(); i++)
	if (_v[i].extra >= 0)
	    sa << _v[i] << '\n';
    return sa.take_string();
}


int
RadixIPLookup::add_route(const IPRoute& route, bool set, IPRoute* old_route, ErrorHandler *)
{
    int found = _v.size();
    for (int i = 0; i < _v.size(); i++)
	if (_v[i].extra < 0)
	    found = i;
	else if (_v[i].addr == route.addr && _v[i].mask == route.mask) {
	    if (!set)
		return -EEXIST;
	    if (old_route)
		*old_route = _v[i];
	    _v[i].gw = route.gw;
	    _v[i].port = route.port;
	    return 0;
	}

    if (found == _v.size())
	_v.push_back(route);
    else
	_v[found] = route;
    uint32_t hmask = ntohl(route.mask.addr());
    if (!hmask)
	_default_key = found;
    else {
	Radix* r = _radix->change(ntohl(route.addr.addr()), ~hmask + 1, found, hmask);
	_v[found].extra = r->_route_index;
	r->_route_index = found;
    }
    return 0;
}

int
RadixIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler*)
{
    for (int i = 0; i < _v.size(); i++)
	if (_v[i].extra >= 0 && _v[i].addr == route.addr && _v[i].mask == route.mask) {
	    if (old_route)
		*old_route = _v[i];
	    uint32_t hmask = ntohl(route.mask.addr());
	    if (!hmask)
		_default_key = -1;
	    else {
		Radix* r = _radix->change(ntohl(route.addr.addr()), ~hmask + 1, -1, hmask);
		// add back less specific routes
		int32_t* pprev = &r->_route_index;
		for (int32_t j = *pprev; j < 0x7FFFFFFF; j = *pprev)
		    if (j == i)
			*pprev = _v[i].extra;
		    else {
			if (_v[j].contains(_v[i])) {
			    hmask = ntohl(_v[j].mask.addr());
			    (void) _radix->change(ntohl(_v[j].addr.addr()), ~hmask + 1, j, hmask);
			}
			pprev = &_v[j].extra;
		    }
	    }
	    _v[i].extra = -1;
	    return 0;
	}

    return -ENOENT;
}

int
RadixIPLookup::lookup_route(IPAddress addr, IPAddress &gw) const
{
    int key = Radix::lookup(_radix, _default_key, ntohl(addr.addr()));
    if (key >= 0 && _v[key].contains(addr)) {
	gw = _v[key].gw;
	return _v[key].port;
    } else {
	gw = 0;
	return -1;
    }
}

// generate Vector template instance
#include <click/vector.cc>
//template class Vector<RadixIPLookup::Entry>;
CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(RadixIPLookup)

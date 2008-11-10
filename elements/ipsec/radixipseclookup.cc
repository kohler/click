// -*- c-basic-offset: 4 -*-
/*
 * radixipseclookup.{cc,hh} -- looks up next-hop address in radix table
 * Small Changes to add_route to support IPSEC ESP.
 * Dimitris Syrivelis
 * Copyright (c) 2006 University of Thessaly
 *
 * Based on radixipseclookup.{cc,hh} -- looks up next-hop address in radix table
 * Eddie Kohler (earlier versions: Thomer M. Gil, Benjie Chen)
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
#include "radixipseclookup.hh"
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

RadixIPsecLookup::Radix*
RadixIPsecLookup::Radix::make_radix(int bitshift, int n)
{
    if (Radix* r = (Radix*) new unsigned char[sizeof(Radix) + n * sizeof(Child)]) {
	r->_route_index = -1;
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

void
RadixIPsecLookup::Radix::free_radix(Radix* r)
{
    if (r->_nchildren)
	for (int i = 0; i < r->_n; i++)
	    if (r->_children[i].child)
		free_radix(r->_children[i].child);
    delete[] (unsigned char *)r;
}

RadixIPsecLookup::Radix*
RadixIPsecLookup::Radix::change(uint32_t addr, uint32_t naddr, int key, uint32_t key_priority)
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


RadixIPsecLookup::RadixIPsecLookup()
    : _vfree(-1), _default_key(-1), _radix(Radix::make_radix(24, 256))
{
}

RadixIPsecLookup::~RadixIPsecLookup()
{
}


void
RadixIPsecLookup::cleanup(CleanupStage)
{
    _v.clear();
    Radix::free_radix(_radix);
    _radix = 0;
}


String
RadixIPsecLookup::dump_routes()
{
    StringAccum sa;
    for (int j = _vfree; j >= 0; j = _v[j].extra)
	_v[j].kill();
    for (int i = 0; i < _v.size(); i++)
	if (!_v[i].addr || _v[i].mask)
	    _v[i].unparse(sa, true) << '\n';
    return sa.take_string();
}


int
RadixIPsecLookup::add_route(const IPsecRoute& route, bool set, IPsecRoute* old_route, ErrorHandler *)
{
    int found;
    if (_vfree < 0) {
	found = _v.size();
	_v.push_back(IPsecRoute());
    } else {
	found = _vfree;
	_vfree = _v[found].extra;
    }
    _v[found] = route;
    _v[found].extra = -1;

    int32_t* pprev;
    uint32_t hmask = ntohl(route.mask.addr());
    if (hmask) {
	Radix* r = _radix->change(ntohl(route.addr.addr()), ~hmask + 1, found, hmask);
	pprev = &r->_route_index;
    } else {
	_default_key = found;
	pprev = &_default_key;
    }

    for (int32_t j = *pprev; j >= 0; j = *pprev)
	if (route.addr == _v[j].addr && route.mask == _v[j].mask) {
	    int r;
	    if (old_route)
		*old_route = _v[j];
	    if (!set) {
		_v[found] = _v[j];
		r = -EEXIST;
	    } else {
		_v[found].extra = _v[j].extra;
		r = 0;
	    }
	    *pprev = found;
	    _v[j].extra = _vfree;
	    _vfree = j;
	    return r;
	} else
	    pprev = &_v[j].extra;

    *pprev = found;
    return 0;
}

int
RadixIPsecLookup::remove_route(const IPsecRoute& route, IPsecRoute* old_route, ErrorHandler*)
{
    int32_t* pprev;
    uint32_t hmask = ntohl(route.mask.addr());
    if (hmask) {
	Radix* r = _radix->change(ntohl(route.addr.addr()), ~hmask + 1, -1, hmask);
	pprev = &r->_route_index;
    } else {
	// no need to set _default_key; will happen, or not, below
	pprev = &_default_key;
    }

    int r = -ENOENT;
    for (int32_t j = *pprev; j >= 0; j = *pprev)
	if (route.match(_v[j])) {
	    if (old_route)
		*old_route = _v[j];
	    *pprev = _v[j].extra;
	    _v[j].extra = _vfree;
	    _vfree = j;
	    r = 0;
	} else {
	    if (_v[j].contains(route) && (hmask = ntohl(_v[j].mask.addr())))
		(void) _radix->change(ntohl(_v[j].addr.addr()), ~hmask + 1, j, hmask);
	    pprev = &_v[j].extra;
	}

    return r;
}

int
RadixIPsecLookup::lookup_route(IPAddress addr, IPAddress &gw, unsigned int &spi, SADataTuple* &sa_data) const
{
    int key = Radix::lookup(_radix, _default_key, ntohl(addr.addr()));
    if (key >= 0 && _v[key].contains(addr)) {
	gw = _v[key].gw;
	spi = _v[key].spi;
	sa_data = _v[key].sa_data;
	return _v[key].port;
    } else {
	gw = 0;
	return -1;
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPsecRouteTable)
EXPORT_ELEMENT(RadixIPsecLookup)

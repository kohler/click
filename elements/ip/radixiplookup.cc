// -*- c-basic-offset: 4 -*-
/*
 * radixiplookup.{cc,hh} -- looks up next-hop address in radix table
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
#include "radixiplookup.hh"
CLICK_DECLS

class RadixIPLookup::Radix { public:

    static Radix *make_radix(int bitshift, int n);
    static void free_radix(Radix *r);

    int change(uint32_t addr, uint32_t mask, int key, bool set);

    static inline int lookup(const Radix *r, int cur, uint32_t addr) {
	while (r) {
	    int i1 = (addr >> r->_bitshift) & (r->_n - 1);
	    const Child &c = r->_children[i1];
	    if (c.key)
		cur = c.key;
	    r = c.child;
	}
	return cur;
    }

  private:

    int _bitshift;
    int _n;
    int _nchildren;
    struct Child {
	int key;
	Radix *child;
    } _children[0];

    Radix()			{ }
    ~Radix()			{ }

    int &key_for(int i) {
	assert(i >= 2 && i < _n * 2);
	if (i >= _n)
	    return _children[i - _n].key;
	else {
	    int *x = reinterpret_cast<int *>(_children + _n);
	    return x[i - 2];
	}
    }

    friend class RadixIPLookup;

};

RadixIPLookup::Radix*
RadixIPLookup::Radix::make_radix(int bitshift, int n)
{
    if (Radix* r = (Radix*) new unsigned char[sizeof(Radix) + n * sizeof(Child) + (n - 2) * sizeof(int)]) {
	r->_bitshift = bitshift;
	r->_n = n;
	r->_nchildren = 0;
	memset(r->_children, 0, n * sizeof(Child) + (n - 2) * sizeof(int));
	return r;
    } else
	return 0;
}

void
RadixIPLookup::Radix::free_radix(Radix* r)
{
    if (r->_nchildren)
	for (int i = 0; i < r->_n; i++)
	    if (r->_children[i].child)
		free_radix(r->_children[i].child);
    delete[] (unsigned char *)r;
}

int
RadixIPLookup::Radix::change(uint32_t addr, uint32_t mask, int key, bool set)
{
    int i1 = (addr >> _bitshift) & (_n - 1);

    // check if change only affects children
    if (mask & ((1U << _bitshift) - 1)) {
	if (!_children[i1].child
	    && (_children[i1].child = make_radix(_bitshift - 4, 16)))
	    ++_nchildren;
	if (_children[i1].child)
	    return _children[i1].child->change(addr, mask, key, set);
	else
	    return 0;
    }

    // find current key
    i1 = _n + i1;
    int nmasked = _n - ((mask >> _bitshift) & (_n - 1));
    for (int x = nmasked; x > 1; x /= 2)
	i1 /= 2;
    int replace_key = key_for(i1), prev_key = replace_key;
    if (prev_key && i1 > 3 && key_for(i1 / 2) == prev_key)
	prev_key = 0;

    // replace previous key with current key, if appropriate
    if (!key && i1 > 3)
	key = key_for(i1 / 2);
    if (prev_key != key && (!prev_key || set)) {
	for (nmasked = 1; i1 < _n * 2; i1 *= 2, nmasked *= 2)
	    for (int x = i1; x < i1 + nmasked; ++x)
		if (key_for(x) == replace_key)
		    key_for(x) = key;
    }
    return prev_key;
}


RadixIPLookup::RadixIPLookup()
    : _vfree(-1), _default_key(0), _radix(Radix::make_radix(24, 256))
{
}

RadixIPLookup::~RadixIPLookup()
{
}


void
RadixIPLookup::cleanup(CleanupStage)
{
    _v.clear();
    Radix::free_radix(_radix);
    _radix = 0;
}


String
RadixIPLookup::dump_routes()
{
    StringAccum sa;
    for (int j = _vfree; j >= 0; j = _v[j].extra)
	_v[j].kill();
    for (int i = 0; i < _v.size(); i++)
	if (_v[i].real())
	    _v[i].unparse(sa, true) << '\n';
    return sa.take_string();
}


int
RadixIPLookup::add_route(const IPRoute &route, bool set, IPRoute *old_route, ErrorHandler *)
{
    int found = (_vfree < 0 ? _v.size() : _vfree), last_key;
    if (route.mask) {
	uint32_t addr = ntohl(route.addr.addr());
	uint32_t mask = ntohl(route.mask.addr());
	last_key = _radix->change(addr, mask, found + 1, set);
    } else {
	last_key = _default_key;
	if (!last_key || set)
	    _default_key = found + 1;
    }

    if (last_key && old_route)
	*old_route = _v[last_key - 1];
    if (last_key && !set)
	return -EEXIST;

    if (found == _v.size())
	_v.push_back(route);
    else {
	_vfree = _v[found].extra;
	_v[found] = route;
    }
    _v[found].extra = -1;

    if (last_key) {
	_v[last_key - 1].extra = _vfree;
	_vfree = last_key - 1;
    }

    return 0;
}

int
RadixIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler*)
{
    int last_key;
    if (route.mask) {
	uint32_t addr = ntohl(route.addr.addr());
	uint32_t mask = ntohl(route.mask.addr());
	// NB: this will never actually make changes
	last_key = _radix->change(addr, mask, 0, false);
    } else
	last_key = _default_key;

    if (last_key && old_route)
	*old_route = _v[last_key - 1];
    if (!last_key || !route.match(_v[last_key - 1]))
	return -ENOENT;
    _v[last_key - 1].extra = _vfree;
    _vfree = last_key - 1;

    if (route.mask) {
	uint32_t addr = ntohl(route.addr.addr());
	uint32_t mask = ntohl(route.mask.addr());
	(void) _radix->change(addr, mask, 0, true);
    } else
	_default_key = 0;
    return 0;
}

int
RadixIPLookup::lookup_route(IPAddress addr, IPAddress &gw) const
{
    int key = Radix::lookup(_radix, _default_key, ntohl(addr.addr()));
    if (key) {
	gw = _v[key - 1].gw;
	return _v[key - 1].port;
    } else {
	gw = 0;
	return -1;
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(RadixIPLookup)

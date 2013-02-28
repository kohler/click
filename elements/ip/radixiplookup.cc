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

    static Radix *make_radix(int level);
    static void free_radix(Radix *r, int level);

    int change(uint32_t addr, uint32_t mask, int key, bool set, int level);


    static inline int lookup(const Radix *r, int cur, uint32_t addr, int level) {
	while (r) {
	    int i1 = (addr >> _bitshift[level]) & (_nbuckets[level] - 1);
	    const Child &c = r->_children[i1];	   
	    if (c.key)
		cur = c.key;
	    r = c.child;
	    level++;
	}
	return cur;
    }
    
private:


    struct Child {
	int key;
	Radix *child;
    } _children[0];

    Radix()			{ }
    ~Radix()			{ }

    int & key_for(int i, int level) {
	int n = _nbuckets[level];
	assert(i >= 2 && i < n * 2);
	if (i >= n)
	    return _children[i - n].key;
	else {
	    int *x = reinterpret_cast<int *>(_children + n);
	    return x[i - 2];
	}
    }
       
    static const int _bitshift [5];
    static const int _nbuckets [5];

    friend class RadixIPLookup;

};

const int RadixIPLookup::Radix::_bitshift [5] = {16, 12, 8, 4, 0};

// The number of buckets each node contains
// 2^16 at the first level and 2^4 at 4 subsequent levels.
// (2^16).(2^4)^4 = 2^32.
const int RadixIPLookup::Radix::_nbuckets [5] = {65536, 16, 16, 16, 16};
    
int
RadixIPLookup::find_lookup_key(IPAddress gw, int32_t port) {
    for(int i=0; i  < _lookup.size(); i++) {
	if(_lookup[i].gw == gw  &&
	   _lookup[i].port == port) 
	    return (i + 1);
    }
    return 0;
}

RadixIPLookup::Radix*
RadixIPLookup::Radix::make_radix(int level)
{
    int n = _nbuckets[level];
    if (Radix* r = (Radix*) new unsigned char[sizeof(Radix) + n * sizeof(Child) + (n - 2) * sizeof(int)]) {
	memset(r->_children, 0, n * sizeof(Child) + (n - 2) * sizeof(int));
	return r;
    } else
	return 0;
}

void
RadixIPLookup::Radix::free_radix(Radix* r, int level)
{
    int n = _nbuckets[level];
    for (int i = 0; i < n; i++)
	if (r->_children[i].child)
	    free_radix(r->_children[i].child, level+1);
    delete[] (unsigned char *)r;
}

int
RadixIPLookup::Radix::change(uint32_t addr, uint32_t mask, int key, bool set, int level)
{
    int shift = _bitshift[level];
    int n = _nbuckets[level];
    int i1 = (addr >> shift) & (n - 1);

    // check if change only affects children
    if (mask & ((1U << shift) - 1)) {
	if (!_children[i1].child
	    && (_children[i1].child = make_radix(level + 1))) {
	    ;
	}
	if (_children[i1].child)
	    return _children[i1].child->change(addr, mask, key, set, level+1);
	else
	    return 0;
    }

    // find current key
    i1 = n + i1;
    int nmasked = n - ((mask >> shift) & (n - 1));
    for (int x = nmasked; x > 1; x /= 2)
	i1 /= 2;
    int replace_key = key_for(i1, level), prev_key = replace_key;
    if (prev_key && i1 > 3 && key_for(i1 / 2, level) == prev_key)
     	prev_key = 0;

    // replace previous key with current key, if appropriate
    if (!key && i1 > 3)
	key = (key_for(i1 / 2, level));

    if (prev_key != key && (!prev_key || set)) {
	for (nmasked = 1; i1 < n * 2; i1 *= 2, nmasked *= 2)
	    for (int x = i1; x < i1 + nmasked; ++x) {
		if (key_for(x, level) == replace_key) {
		    key_for(x, level) = key;		    		    
		}
	    }
	
    }
    return prev_key;
}


RadixIPLookup::RadixIPLookup()
    : _vfree(-1), _default_key(0), _radix(Radix::make_radix(0))
{
}

RadixIPLookup::~RadixIPLookup()
{
}


void
RadixIPLookup::cleanup(CleanupStage)
{
    int level = 0;
    _v.clear();
    Radix::free_radix(_radix, level);
    _radix = 0;
}

void
RadixIPLookup::add_handlers()
{
    IPRouteTable::add_handlers();
    add_write_handler("flush", flush_handler, 0, Handler::BUTTON);
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
    int lookup_key = find_lookup_key(route.gw, route.port);
    if(!lookup_key) 
	lookup_key = _lookup.size() + 1;
		    
    if (route.mask) {
	uint32_t addr = ntohl(route.addr.addr());
	uint32_t mask = ntohl(route.mask.addr());
	int level = 0;
	last_key = _radix->change(addr, mask, combine_key(found + 1, lookup_key), set, level);
	// The key returned by change is the combined key, we need only the _v key.
	last_key = get_key(last_key);
    } else {
	last_key = get_key(_default_key);
	if (!last_key || set)
	    _default_key = combine_key(found + 1, lookup_key);
    }

    if (last_key && old_route)
	*old_route = _v[last_key - 1];
    if (last_key && !set)
	return -EEXIST;

    if (lookup_key == (_lookup.size() + 1)) {
	GWPort gw_port = {route.gw, route.port};
	_lookup.push_back(gw_port);
    }

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
	int level = 0;
	// NB: this will never actually make changes
	last_key = get_key(_radix->change(addr, mask, 0, false, level));
    } else
	last_key = get_key(_default_key);

    if (last_key && old_route)
	*old_route = _v[last_key - 1];
    if (!last_key || !route.match(_v[last_key - 1]))
	return -ENOENT;
    _v[last_key - 1].extra = _vfree;
    _vfree = last_key - 1;

    if (route.mask) {
	uint32_t addr = ntohl(route.addr.addr());
	uint32_t mask = ntohl(route.mask.addr());
	int level = 0;
	(void) _radix->change(addr, mask, 0, true, level);
    } else
	_default_key = 0;
    return 0;
}

int
RadixIPLookup::lookup_route(IPAddress addr, IPAddress &gw) const
{
    int level = 0;    
    int key = Radix::lookup(_radix, _default_key, ntohl(addr.addr()), level);
    int lookup_key = get_lookup_key(key);
    if (lookup_key) {
	gw = _lookup[lookup_key - 1].gw;
	return _lookup[lookup_key - 1].port;
    } else {
	gw = 0;
	return -1;
    }
}

void
RadixIPLookup::flush_table()
{
    int level = 0;
    _v.clear();
    Radix::free_radix(_radix, level);
    _radix = Radix::make_radix(0);
    _vfree = -1;
    _default_key = 0;
}

int
RadixIPLookup::flush_handler(const String &, Element *e, void *, ErrorHandler *)
{
    RadixIPLookup *t = static_cast<RadixIPLookup *>(e);
    t->flush_table();
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(RadixIPLookup)

// -*- c-basic-offset: 4 -*-
/*
 * lineariplookup.{cc,hh} -- element looks up next-hop address in linear
 * routing table
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include "lineariplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

LinearIPLookup::LinearIPLookup()
{
    add_input();
}

LinearIPLookup::~LinearIPLookup()
{
}

void
LinearIPLookup::notify_noutputs(int n)
{
    set_noutputs(n);
}

int
LinearIPLookup::initialize(ErrorHandler *)
{
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = _last_addr;
#endif
    return 0;
}

bool
LinearIPLookup::check() const
{
    bool ok = true;
    
    // 'next' pointers are correct
    for (int i = 0; i < _t.size(); i++) {
	for (int j = i + 1; j < _t[i].next && j < _t.size(); j++)
	    if (_t[i].contains(_t[j])) {
		click_chatter("%s: bad next pointers: routes %s, %s", declaration().cc(), _t[i].unparse_addr().cc(), _t[j].unparse_addr().cc());
		ok = false;
	    }
	if (_t[i].next < _t.size())
	    if (!_t[i].contains(_t[ _t[i].next ])) {
		click_chatter("%s: bad next pointers: routes %s, %s", declaration().cc(), _t[i].unparse_addr().cc(), _t[ _t[i].next ].unparse_addr().cc());
		ok = false;
	    }
    }

    // no duplicate routes
    for (int i = 0; i < _t.size(); i++)
	for (int j = i + 1; j < _t.size(); j++)
	    if (_t[i].addr == _t[j].addr && _t[i].mask == _t[j].mask) {
		click_chatter("%s: duplicate routes for %s", declaration().cc(), _t[i].unparse_addr().cc());
		ok = false;
	    }

    // caches point to the right place
    if (_last_addr && lookup_entry(_last_addr) != _last_entry) {
	click_chatter("%s: bad cache entry for %s", declaration().cc(), _last_addr.unparse().cc());
	ok = false;
    }
#ifdef IP_RT_CACHE2
    if (_last_addr2 && lookup_entry(_last_addr2) != _last_entry2) {
	click_chatter("%s: bad cache entry for %s", declaration().cc(), _last_addr2.unparse().cc());
	ok = false;
    }
#endif
    
    return ok;
}

int
LinearIPLookup::add_route(const IPRoute &r, ErrorHandler *)
{
    // overwrite any existing route
    for (int i = 0; i < _t.size(); i++)
	if (_t[i].addr == r.addr && _t[i].mask == r.mask) {
	    _t[i].gw = r.gw;
	    _t[i].port = r.port;
	    check();
	    return 0;
	}

    // put it in a slot
    Entry e(r);
    int found = -1;
    for (int i = 0; i < _t.size(); i++)
	if (_t[i].addr && !_t[i].mask) {
	    found = i;
	    _t[i] = e;
	    break;
	}
    if (found < 0) {
	found = _t.size();
	_t.push_back(e);
    }

    // patch up 'next' pointers
    for (int i = found - 1; i >= 0; i--)
	if (_t[i].contains(r) && _t[i].next > found)
	    _t[i].next = found;
    for (int i = found + 1; i < _t.size(); i++)
	if (r.contains(_t[i])) {
	    _t[found].next = i;
	    break;
	}

    // get rid of caches
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = IPAddress();
#endif
    
    check();
    return 0;
}

int
LinearIPLookup::remove_route(const IPRoute& r, ErrorHandler *errh)
{
    int nremoved = 0;

    // remove routes from main table; replace with redundant route, if any
    for (int i = 0; i < _t.size(); i++) {
	Entry &e = _t[i];
	if (e.addr == r.addr && e.mask == r.mask
	    && (r.port < 0 || (e.gw == r.gw && e.port == r.port && !nremoved))) {
	    e.addr = IPAddress(1);
	    e.mask = IPAddress(0);
	    nremoved++;
	}
    }

    // get rid of caches
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = IPAddress();
#endif
        
    check();
    if (!nremoved)
	errh->warning("no routes removed");
    return 0;
}

int
LinearIPLookup::lookup_entry(IPAddress a) const
{
    for (int i = 0; i < _t.size(); i++)
	if ((a & _t[i].mask) == _t[i].addr) {
	    int found = i;
	    for (int j = _t[i].next; j < _t.size(); j++)
		if ((a & _t[j].mask) == _t[j].addr
		    && _t[j].mask.mask_as_specific(_t[found].mask))
		    found = j;
	    return found;
	}
    return -1;
}

int
LinearIPLookup::lookup_route(IPAddress a, IPAddress &gw) const
{
    int ei = lookup_entry(a);
    if (ei >= 0) {
	gw = _t[ei].gw;
	return _t[ei].port;
    } else
	return -1;
}

StringAccum &
operator<<(StringAccum &sa, const LinearIPLookup::Entry &e)
{
    int l = sa.length();
    sa << e.addr.unparse_with_mask(e.mask) << '\t';
    if (sa.length() < l + 17)
	sa << '\t';
    l = sa.length();
    sa << e.gw << '\t';
    if (sa.length() < l + 9)
	sa << '\t';
    sa << e.port;
    return sa;
}

String
LinearIPLookup::dump_routes() const
{
    StringAccum sa;
    for (int i = 0; i < _t.size(); i++)
	if (!_t[i].addr || _t[i].mask)
	    sa << _t[i] << '\n';
    return sa.take_string();
}

void
LinearIPLookup::push(int, Packet *p)
{
#define EXCHANGE(a,b,t) { t = a; a = b; b = t; }
    IPAddress a = p->dst_ip_anno();
    int ei = -1;

    if (a && a == _last_addr)
	ei = _last_entry;
#ifdef IP_RT_CACHE2
    else if (a && a == _last_addr2)
	ei = _last_entry2;
#endif
    else if ((ei = lookup_entry(a)) >= 0) {
#ifdef IP_RT_CACHE2
	_last_addr2 = _last_addr;
	_last_entry2 = _last_entry;
#endif
	_last_addr = a;
	_last_entry = ei;
    } else {
	click_chatter("LinearIPLookup: no gw for %x", a.addr());
	p->kill();
	return;
    }

    const Entry &e = _t[ei];
    if (e.gw)
	p->set_dst_ip_anno(e.gw);
    output(e.port).push(p);
}

void
LinearIPLookup::add_handlers()
{
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("remove", remove_route_handler, 0);
    add_write_handler("ctrl", ctrl_handler, 0);
    add_read_handler("table", table_handler, 0);
}

#include <click/vector.cc>
CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(LinearIPLookup)

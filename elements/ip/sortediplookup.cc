// -*- c-basic-offset: 4 -*-
/*
 * sortediplookup.{cc,hh} -- element looks up next-hop address in sorted
 * routing table
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2005 Regents of the University of California
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
#include "sortediplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

SortedIPLookup::SortedIPLookup()
{
}

SortedIPLookup::~SortedIPLookup()
{
}

int
SortedIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    errh->warning("SortedIPLookup is deprecated; use LinearIPLookup or RadixIPLookup instead");
    return LinearIPLookup::configure(conf, errh);
}

bool
SortedIPLookup::check() const
{
    bool ok = LinearIPLookup::check();

    // next pointers are all as late as possible
    for (int i = 0; i < _t.size(); i++)
	if (_t[i].extra < _t.size()) {
	    click_chatter("%s: route %s has a nontrivial next", declaration().c_str(), _t[i].unparse_addr().c_str());
	    ok = false;
	}

    return ok;
}

void
SortedIPLookup::sort_table()
{
    // topological sort the entries
    if (_t.size() == 0)
	return;

    // First, count dependencies.
    Vector<int> dep(_t.size(), 0);
    int nunreal = 0;
    for (int i = 0; i < _t.size(); i++) {
	if (!_t[i].real())
	    dep[i]++, nunreal++;
	else
	    for (int j = 0; j < _t.size(); j++)
		if (_t[j].contains(_t[i]) && i != j)
		    dep[j]++;
    }

    // Now, create the permutation array.
    Vector<int> permute;
    int first = 0, qpos = 0;
    while (permute.size() < _t.size() - nunreal) {

	// Find something on which nothing depends.
	for (; first < _t.size() && dep[first] != 0; first++)
	    /* nada */;
	assert(first < _t.size());

	// Add it to the queue.
	permute.push_back(first);

	// Go over the queue, adding to 'permute'.
	for (; qpos < permute.size(); qpos++) {
	    int which = permute[qpos];
	    dep[which] = -1;
	    for (int i = 0; i < _t.size(); i++)
		if (dep[i] > 0 && _t[i].contains(_t[which]) && _t[i].real()) {
		    if (!--dep[i])
			permute.push_back(i);
		}
	}

    }

    // Permute the table according to the array.
    Vector<IPRoute> nt(_t);
    for (int i = 0; i < permute.size(); i++) {
	if (permute[i] != i)
	    nt[i] = _t[permute[i]];
	nt[i].extra = 0x7FFFFFFF;
    }
    _t.swap(nt);
    _t.resize(permute.size());
    _zero_route = -1;

    check();
}

int
SortedIPLookup::add_route(const IPRoute& route, bool set, IPRoute* old, ErrorHandler *errh)
{
    int r;
    if ((r = LinearIPLookup::add_route(route, set, old, errh)) < 0)
	return r;
    sort_table();
    return 0;
}

int
SortedIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler *errh)
{
    int r;
    if ((r = LinearIPLookup::remove_route(route, old_route, errh)) < 0)
	return r;
    sort_table();
    return 0;
}


/* Strictly speaking, we could use LinearIPLookup's push() function and
   lookup_entry(). All the next pointers point beyond the table's end, so
   there wouldn't be much difference in terms of performance. */

inline int
SortedIPLookup::lookup_entry(IPAddress a) const
{
    for (int i = 0; i < _t.size(); i++)
	if (_t[i].contains(a))
	    return i;
    return -1;
}

void
SortedIPLookup::push(int, Packet *p)
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
	click_chatter("SortedIPLookup: no gw for %x", a.addr());
	p->kill();
	return;
    }

    const IPRoute &e = _t[ei];
    if (e.gw)
	p->set_dst_ip_anno(e.gw);
    output(e.port).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinearIPLookup)
EXPORT_ELEMENT(SortedIPLookup)

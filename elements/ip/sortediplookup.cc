// -*- c-basic-offset: 4 -*-
/*
 * sortediplookup.{cc,hh} -- element looks up next-hop address in sorted
 * routing table
 * Eddie Kohler
 *
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
#include "sortediplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/error.hh>

SortedIPLookup::SortedIPLookup()
{
    MOD_INC_USE_COUNT;
}

SortedIPLookup::~SortedIPLookup()
{
    MOD_DEC_USE_COUNT;
}

bool
SortedIPLookup::check() const
{
    bool ok = LinearIPLookup::check();

    // 'next' pointers are all as late as possible
    for (int i = 0; i < _t.size(); i++)
	if (_t[i].next < _t.size()) {
	    click_chatter("%s: route %s has a nontrivial next", declaration().cc(), _t[i].unparse_addr().cc());
	    ok = false;
	}

    return ok;
}

static bool
entry_subset(const LinearIPLookup::Entry &a, const LinearIPLookup::Entry &b)
{
    return ((a.addr & b.mask) == b.addr && a.mask.mask_as_long(b.mask));
}

void
SortedIPLookup::sort_table()
{
    // topological sort the entries
    if (_t.size() == 0)
	return;
    
    // First, count dependencies.
    Vector<int> dep(_t.size(), 0);
    for (int i = 0; i < _t.size(); i++)
	for (int j = 0; j < _t.size(); j++)
	    if (entry_subset(_t[i], _t[j]) && i != j)
		dep[j]++;

    // Now, create the permutation array.
    Vector<int> permute;
    int first = 0, qpos = 0;
    while (permute.size() < _t.size()) {

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
		if (dep[i] > 0 && entry_subset(_t[which], _t[i])) {
		    if (!--dep[i])
			permute.push_back(i);
		}
	}

    }

    // Permute the table according to the array.
    Vector<Entry> nt(_t);
    for (int i = 0; i < _t.size(); i++) {
	if (permute[i] != i)
	    nt[i] = _t[permute[i]];
	nt[i].next = 0x7FFFFFFF;
    }
    _t.swap(nt);

    check();
}

int
SortedIPLookup::add_route(IPAddress a, IPAddress m, IPAddress gw, int output, ErrorHandler *errh)
{
    if (LinearIPLookup::add_route(a, m, gw, output, errh) < 0)
	return -1;
    sort_table();
    return 0;
}

int
SortedIPLookup::remove_route(IPAddress a, IPAddress m, IPAddress gw, int output, ErrorHandler *errh)
{
    if (LinearIPLookup::remove_route(a, m, gw, output, errh) < 0)
	return -1;
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
	if ((a & _t[i].mask) == _t[i].addr)
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

    const Entry &e = _t[ei];
    if (e.gw)
	p->set_dst_ip_anno(e.gw);
    output(e.output).push(p);
}

#include <click/vector.cc>

ELEMENT_REQUIRES(LinearIPLookup false)
EXPORT_ELEMENT(SortedIPLookup)

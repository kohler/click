// -*- c-basic-offset: 4; related-file-name: "../include/click/ipaddresslist.hh" -*-
/*
 * ipaddresslist.{cc,hh} -- a list of IP addresses
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/ipaddresslist.hh>
#include <click/glue.hh>
CLICK_DECLS

void
IPAddressList::push_back(IPAddress a)
{
    if (uint32_t *v2 = new uint32_t[_n + 1]) {
	memcpy(v2, _v, _n * sizeof(uint32_t));
	v2[_n] = a;
	delete[] _v;
	_v = v2;
	_n++;
    }
}

void
IPAddressList::insert(IPAddress a)
{
    if (!contains(a))
	push_back(a);
}

extern "C" {
static int
ipaddress_compar(const void *va, const void *vb)
{
    return *reinterpret_cast<const uint32_t *>(va) - *reinterpret_cast<const uint32_t *>(vb);
}
}

void
IPAddressList::sort()
{
    click_qsort(_v, _n, sizeof(uint32_t), ipaddress_compar);
}

CLICK_ENDDECLS

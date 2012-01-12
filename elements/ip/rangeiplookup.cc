// -*- c-basic-offset: 4 -*-
/*
 * rangeiplookup.{cc,hh} -- binary search for output port and next-hop gateway
 * in a very compact sorted array, aiming for high CPU cache hit ratios
 * Marko Zec
 *
 * Copyright (c) 2005 International Computer Science Institute
 * Copyright (c) 2005 University of Zagreb
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
#include "rangeiplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

RangeIPLookup::RangeIPLookup()
    : _range_base((uint32_t *) CLICK_LALLOC((1 << KICKSTART_BITS) * sizeof(uint32_t))),
      _range_len((uint32_t *) CLICK_LALLOC((1 << KICKSTART_BITS) * sizeof(uint32_t))),
      _range_t((uint32_t *) CLICK_LALLOC(RANGES_MAX * sizeof(uint32_t))),
      _active(false)
{
}

RangeIPLookup::~RangeIPLookup()
{
    CLICK_LFREE(_range_base, (1 << KICKSTART_BITS) * sizeof(uint32_t));
    CLICK_LFREE(_range_len, (1 << KICKSTART_BITS) * sizeof(uint32_t));
    CLICK_LFREE(_range_t, RANGES_MAX * sizeof(uint32_t));
}

int
RangeIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int r;
    if ((r = _helper.initialize()) < 0)
	return r;
    flush_table();
    return IPRouteTable::configure(conf, errh);
}

int
RangeIPLookup::initialize(ErrorHandler *)
{
    expand();
    _active = true;
    return 0;
}

void
RangeIPLookup::cleanup(CleanupStage)
{
    _helper.cleanup();
}

void
RangeIPLookup::push(int, Packet *p)
{
    IPAddress gw;
    int port = lookup_route(p->dst_ip_anno(), gw);

#ifdef RANGEIPLOOKUP_VERBOSE
    // Consistency check - does directiplookup yied the same result?
    IPAddress gw1;
    int port1 = _helper.lookup_route(p->dst_ip_anno(), gw1);
    if (port != port1 || gw != gw1)
	click_chatter("RangeIPLookup: consistency check failed!");
#endif

    if (port >= 0) {
        if (gw)
            p->set_dst_ip_anno(gw);
        output(port).push(p);
    } else
        p->kill();
}

int
RangeIPLookup::lookup_route(IPAddress dest, IPAddress &gw) const
{
    uint32_t ip_addr = ntohl(dest.addr());
    uint32_t lowerbound, upperbound, middle;
    uint32_t i = ip_addr >> RANGE_SHIFT; // kickstart table index = MS bits
    uint16_t vport_i;

    lowerbound = _range_base[i];
    upperbound = lowerbound + _range_len[i];
    i = ip_addr & RANGE_MASK;		// Compare only masked LS bits

    // Binary search for a matching range
    while (upperbound > lowerbound) {
	middle = (upperbound + lowerbound) >> 1;
	if (i < (_range_t[middle] & RANGE_MASK))
	    upperbound = middle;
	else if (i < (_range_t[middle + 1] & RANGE_MASK)) {
	    lowerbound = middle;
	    break;
	} else
	    lowerbound = middle + 1;
    }

    // MS bits of the found range contain an index into the output port table
    vport_i = _range_t[lowerbound] >> RANGE_SHIFT;
    gw = _helper._vport[vport_i].gw;
    return _helper._vport[vport_i].port;
}

void
RangeIPLookup::add_handlers()
{
    IPRouteTable::add_handlers();
    add_write_handler("flush", flush_handler, 0, Handler::BUTTON);
}

int
RangeIPLookup::add_route(const IPRoute& route, bool allow_replace, IPRoute* old_route, ErrorHandler *errh)
{
    int error = _helper.add_route(route, allow_replace, old_route, errh);
    if (error == 0 && _active)
	expand();
    return error;
}

int
RangeIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler *errh)
{
    int error = _helper.remove_route(route, old_route, errh);
    if (error == 0 && _active)
	expand();
    return error;
}

/*
 * On each routing table update, we distill the address range based lookup
 * table from the structures provided by the DirectIPLookup class.
 * The main cost of this operation is associated with traversing through
 * 32 + 16 = 48 MBytes of directiplookup tables.  We should implement a
 * more efficient method for updating range-based lookup structures in
 * the future, which would not depend on huge directiplookup tables.
 */
void
RangeIPLookup::expand()
{
    uint32_t range_t_index = 0;
    uint32_t tbl_0_23_index = 0;
    uint32_t range_base;
    uint32_t range_len;

    for (range_base = 0; range_base < (1 << KICKSTART_BITS); range_base++) {
	uint16_t vport_i, vport_i1;

	vport_i = 0xffff;       // Duh!
	_range_base[range_base] = range_t_index;

	for (range_len = 0;
	  tbl_0_23_index < ((range_base + 1) << (24 - KICKSTART_BITS));
	  tbl_0_23_index++) {
	    if (_helper._tbl_0_23[tbl_0_23_index] & 0x8000) {
		uint32_t tbl_24_31_index, j;
		tbl_24_31_index =
			(_helper._tbl_0_23[tbl_0_23_index] & 0x7fff) << 8;
		for (j = 0; j < 256; j++) {
		    vport_i1 = _helper._tbl_24_31[tbl_24_31_index + j];
		    if (vport_i != vport_i1) {
			vport_i = vport_i1;
			_range_t[range_t_index] =
					vport_i << (32 - KICKSTART_BITS) |
					(((tbl_0_23_index << 8) + j) &
					(0xffffffff >> KICKSTART_BITS));
			range_t_index++;
			range_len++;
		    }
		}
	    } else {
		vport_i1 = _helper._tbl_0_23[tbl_0_23_index];
		if (vport_i != vport_i1) {
		    vport_i = vport_i1;
		    _range_t[range_t_index] =
					vport_i << (32 - KICKSTART_BITS) |
					((tbl_0_23_index << 8) &
					(0xffffffff >> KICKSTART_BITS));
		    range_t_index++;
		    range_len++;
		}
	    }
	}
	_range_len[range_base] = range_len - 1;
    }

#ifdef RANGEIPLOOKUP_VERBOSE
    click_chatter("Range expansion done: %d ranges using %d + %d bytes",
		  range_t_index, sizeof(_range_base) + sizeof(_range_len),
		  range_t_index * sizeof(uint32_t));
#endif
}

void
RangeIPLookup::flush_table()
{
    _helper.flush();
    memset(_range_base, 0, (1 << KICKSTART_BITS) * sizeof(*_range_base));
    memset(_range_len, 0, (1 << KICKSTART_BITS) * sizeof(*_range_len));
    memset(_range_t, 0, RANGES_MAX * sizeof(*_range_t));
}

int
RangeIPLookup::flush_handler(const String &, Element *e, void *,
                                ErrorHandler *)
{
    RangeIPLookup *t = static_cast<RangeIPLookup *>(e);
    t->flush_table();
    return 0;
}

String
RangeIPLookup::dump_routes()
{
    return _helper.dump();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DirectIPLookup)
EXPORT_ELEMENT(RangeIPLookup)

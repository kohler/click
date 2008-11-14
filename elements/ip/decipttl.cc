/*
 * decipttl.{cc,hh} -- element decrements IP packet's time-to-live
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "decipttl.hh"
#include <clicknet/ip.h>
#include <click/glue.hh>
CLICK_DECLS

DecIPTTL::DecIPTTL()
{
    _drops = 0;
}

DecIPTTL::~DecIPTTL()
{
}

Packet *
DecIPTTL::simple_action(Packet *p)
{
    WritablePacket *q = p->uniqueify();
    if (!q)
	return 0;

    assert(q->has_network_header());
    click_ip *ip = q->ip_header();
    if (unlikely(ip->ip_ttl <= 1)) {
	++_drops;
	checked_output_push(1, q);
	return 0;

    } else {
	--ip->ip_ttl;

	// 19.Aug.1999 - incrementally update IP checksum as suggested by SOSP
	// reviewers, according to RFC1141, as updated by RFC1624.
	// new_sum = ~(~old_sum + ~old_halfword + new_halfword)
	//         = ~(~old_sum + ~old_halfword + (old_halfword - 0x0100))
	//         = ~(~old_sum + ~old_halfword + old_halfword + ~0x0100)
	//         = ~(~old_sum + ~0 + ~0x0100)
	//         = ~(~old_sum + 0xFEFF)
	unsigned long sum = (~ntohs(ip->ip_sum) & 0xFFFF) + 0xFEFF;
	ip->ip_sum = ~htons(sum + (sum >> 16));

	return q;
    }
}

void
DecIPTTL::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DecIPTTL)
ELEMENT_MT_SAFE(DecIPTTL)

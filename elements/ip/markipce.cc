/*
 * markipce.{cc,hh} -- element marks IP header ECN CE bit
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "markipce.hh"
#include <clicknet/ip.h>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

MarkIPCE::MarkIPCE()
{
    _drops = 0;
}

MarkIPCE::~MarkIPCE()
{
}

int
MarkIPCE::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _force = false;
    return Args(conf, this, errh).read_p("FORCE", _force).complete();
}

Packet *
MarkIPCE::simple_action(Packet *p)
{
    assert(p->has_network_header());
    const click_ip *iph = p->ip_header();
    if ((iph->ip_tos & IP_ECNMASK) == IP_ECN_NOT_ECT && !_force) {
	p->kill();
	return 0;
    } else if ((iph->ip_tos & IP_ECNMASK) == IP_ECN_CE)
	return p;

    WritablePacket *q;
    if (!(q = p->uniqueify()))
	return 0;

    click_ip *q_iph = q->ip_header();
    uint16_t old_hw = *(uint16_t *) q_iph;
    q_iph->ip_tos |= IP_ECN_CE;
    click_update_in_cksum(&q_iph->ip_sum, old_hw, *(uint16_t *) q_iph);

    return q;
}

void
MarkIPCE::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MarkIPCE)
ELEMENT_MT_SAFE(MarkIPCE)

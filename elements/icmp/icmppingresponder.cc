// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * icmppingresponder.{cc,hh} -- element constructs ICMP echo response packets
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "icmppingresponder.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
CLICK_DECLS

ICMPPingResponder::ICMPPingResponder()
{
}

ICMPPingResponder::~ICMPPingResponder()
{
}

Packet *
ICMPPingResponder::simple_action(Packet *p_in)
{
    const click_ip *iph_in = p_in->ip_header();
    const click_icmp *icmph_in = p_in->icmp_header();

    if (p_in->transport_length() < (int) sizeof(click_icmp_sequenced)
	|| iph_in->ip_p != IP_PROTO_ICMP || icmph_in->icmp_type != ICMP_ECHO) {
	if (noutputs() == 2)
	    output(1).push(p_in);
	else
	    p_in->kill();
	return 0;
    }

    WritablePacket *q = p_in->uniqueify();
    if (!q)			// out of memory
	return 0;

    // swap src and target ip addresses (checksum remains valid)
    click_ip *iph = q->ip_header();
    struct in_addr tmp_addr = iph->ip_dst;
    iph->ip_dst = iph->ip_src;
    iph->ip_src = tmp_addr;

    // clear MF, DF, etc.
    // (bug reported by David Scott Page)
    click_update_in_cksum(&iph->ip_sum, iph->ip_off, 0);
    iph->ip_off = 0;

    // set TTL to 255, update checksum
    // (bug reported by <kp13@gmx.co.uk>)
    uint16_t old_hw = ((uint16_t *)iph)[4];
    iph->ip_ttl = 255;
    uint16_t new_hw = ((uint16_t *)iph)[4];
    click_update_in_cksum(&iph->ip_sum, old_hw, new_hw);

    // set annotations
    // (dst_ip_anno bug reported by Sven Hirsch <hirschs@gmx.de>)
    q->set_dst_ip_anno(iph->ip_dst);
    q->timestamp_anno().assign_now();
    SET_PAINT_ANNO(q, 0);

    // set ICMP packet type to ICMP_ECHOREPLY and recalculate checksum
    click_icmp_echo *icmph = reinterpret_cast<click_icmp_echo *>(q->icmp_header());
    old_hw = ((uint16_t *)icmph)[0];
    icmph->icmp_type = ICMP_ECHOREPLY;
    icmph->icmp_code = 0;
    new_hw = ((uint16_t *)icmph)[0];
    click_update_in_cksum(&icmph->icmp_cksum, old_hw, new_hw);
    click_update_zero_in_cksum(&icmph->icmp_cksum, q->transport_header(), q->transport_length());

    return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPPingResponder)
ELEMENT_MT_SAFE(ICMPPingResponder)

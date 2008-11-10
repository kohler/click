// -*- c-basic-offset: 4 -*-
/*
 * eraseippayload.{cc,hh} -- element erases IP packet's payload
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "eraseippayload.hh"
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

EraseIPPayload::EraseIPPayload()
{
}

EraseIPPayload::~EraseIPPayload()
{
}

Packet *
EraseIPPayload::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    if (!p)
	return 0;

    click_ip *ip = p->ip_header();
    if (ip->ip_p == IP_PROTO_TCP) {
	int off = p->transport_header_offset() + (p->tcp_header()->th_off << 2);
	if (off < (int) p->length())
	    memset(p->data() + off, 0, p->length() - off);
	if (p->transport_header_offset() + 18 <= (int) p->length())
	    p->tcp_header()->th_sum = 0;
    } else if (ip->ip_p == IP_PROTO_UDP) {
	int off = p->transport_header_offset() + sizeof(click_udp);
	if (off < (int) p->length())
	    memset(p->data() + off, 0, p->length() - off);
	if (p->transport_header_offset() + 8 <= (int) p->length())
	    p->udp_header()->uh_sum = 0;
    } else if (ip->ip_p == IP_PROTO_ICMP) {
	// XXX no erasing
    } else {
	p->kill();
	return 0;
    }

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EraseIPPayload)
ELEMENT_MT_SAFE(EraseIPPayload)

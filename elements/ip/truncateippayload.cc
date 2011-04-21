// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * truncateippayload.{cc,hh} -- drop packet payload
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "truncateippayload.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
CLICK_DECLS

TruncateIPPayload::TruncateIPPayload()
{
}

TruncateIPPayload::~TruncateIPPayload()
{
}

int
TruncateIPPayload::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t nbytes = 0;
    bool transport = true;
    bool extra_length = true;
    if (Args(conf, this, errh)
	.read_p("LENGTH", nbytes)
	.read_p("TRANSPORT", transport)
	.read("EXTRA_LENGTH", extra_length).complete() < 0)
	return -1;
    _nbytes = (nbytes << 2) + transport + (extra_length << 1);
    return 0;
}

Packet *
TruncateIPPayload::simple_action(Packet *p)
{
    const click_ip *iph = p->ip_header();
    unsigned nbytes = _nbytes >> 2;

    if (!p->has_network_header()) {
	if (p->length() <= nbytes)
	    return p;
	nbytes = p->length() - nbytes;
	goto take;
    } else if (iph->ip_hl < (sizeof(click_ip) >> 2))
	// broken IP header
	nbytes += sizeof(click_ip);
    else {
	nbytes += iph->ip_hl << 2;
	if ((_nbytes & 1) && p->network_length() >= 10)
	    switch (iph->ip_p) {
	    case IP_PROTO_TCP:
		if (p->transport_length() >= 12
		    && p->tcp_header()->th_off >= (sizeof(click_tcp) >> 2))
		    nbytes += p->tcp_header()->th_off << 2;
		else
		    nbytes += sizeof(click_tcp);
		break;
	    case IP_PROTO_UDP:
		nbytes += sizeof(click_udp);
		break;
	    case IP_PROTO_ICMP:
		if (p->transport_length() >= 8)
		    nbytes += click_icmp_hl(p->icmp_header()->icmp_type);
		break;
	    }
    }

    if (p->network_length() <= (int) nbytes)
	return p;
    nbytes = p->network_length() - nbytes;

  take:
    if (_nbytes & 2)
	SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + nbytes);
    p->take(nbytes);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TruncateIPPayload)
ELEMENT_MT_SAFE(TruncateIPPayload)

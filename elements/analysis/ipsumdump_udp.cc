// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_udp.{cc,hh} -- IP transport summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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

#include "ipsumdumpinfo.hh"
#include <click/packet.hh>
#include <click/nameinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/confparse.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_UDP_LEN };

static bool udp_extract(PacketDesc& d, int thunk)
{
    int transport_length = d.p->transport_length();
    switch (thunk & ~B_TYPEMASK) {
	
#define CHECK(l) do { if (!d.udph || transport_length < (l)) return field_missing(d, IP_PROTO_UDP, (l)); } while (0)
	
      case T_UDP_LEN:
	CHECK(6);
	d.v = ntohs(d.udph->uh_ulen);
	return true;
	
#undef CHECK

      default:
	return false;
    }
}

static void udp_inject(PacketOdesc& d, int thunk)
{
    if (!d.make_ip(0) || !d.make_transp())
	return;
    int ip_p = d.p->ip_header()->ip_p;
    if (ip_p && ip_p != IP_PROTO_UDP && ip_p != IP_PROTO_UDPLITE)
	return;
    if (d.p->transport_length() < (int) sizeof(click_udp)
	&& !(d.p = d.p->put(sizeof(click_udp) - d.p->transport_length())))
	return;

    switch (thunk & ~B_TYPEMASK) {
    case T_UDP_LEN:
	d.p->udp_header()->uh_ulen = htons(d.v);
	break;
    }
}

void udp_register_unparsers()
{
    register_field("udp_len", T_UDP_LEN | B_4, ip_prepare, order_transp,
		   udp_extract, udp_inject, num_outa, num_ina, outb, inb);
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_UDP)
CLICK_ENDDECLS

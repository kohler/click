/*
 * lispdecap.{cc,hh} -- element removes the LISP/UDP/IP encapsulation (RFC 9300)
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
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
#include <click/args.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/error.hh>
#include <clicknet/udp.h>
#include <clicknet/lisp.h>
#include "lispdecap.hh"

CLICK_DECLS

/******************************************************************************************************/

LISPDecap::LISPDecap()
{
    _packet_count = 0;
    _drops = 0;
}

/******************************************************************************************************/

LISPDecap::~LISPDecap()
{
}

/******************************************************************************************************/

int
LISPDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read_p("src_adr",
		_my_addr) // optional, unused (kept for old configurations)
	    .complete()
	< 0)
	return -1;
    return 0;
}

/******************************************************************************************************/

void
LISPDecap::push(int port, Packet *p)
{
    if (!port) {
	// Outer IP header: the annotation if CheckIPHeader/MarkIPHeader ran,
	// otherwise the start of the packet.
	const click_ip *iph = p->has_network_header()
	    ? p->ip_header()
	    : reinterpret_cast<const click_ip *>(p->data());
	int ip_off = reinterpret_cast<const uint8_t *>(iph) - p->data();
	int hl = iph->ip_hl << 2;
	int off = ip_off + hl + sizeof(click_udp) + sizeof(click_lisp);
	const click_udp *udph = reinterpret_cast<const click_udp *>(
	    reinterpret_cast<const uint8_t *>(iph) + hl);
	if (ip_off < 0 || (int)p->length() < ip_off + (int)sizeof(click_ip)
	    || hl < (int)sizeof(click_ip)
	    || (int)p->length() < off + (int)sizeof(click_ip)
	    || iph->ip_p != IP_PROTO_UDP
	    || udph->uh_dport != htons(LISP_DATA_PORT)) {
	    ++_drops;
	    checked_output_push(1, p); // not a LISP data packet
	    return;
	}
	++_packet_count;
	p->pull(off);
	const click_ip *inner = reinterpret_cast<const click_ip *>(p->data());
	p->set_ip_header(inner, inner->ip_hl << 2);
	output(0).push(p);
    } else
	p->kill();
}

/******************************************************************************************************/

void
LISPDecap::add_handlers()
{
    add_data_handlers("decap packet", Handler::OP_READ, &_packet_count);
    add_data_handlers(
	"count", Handler::OP_READ, &_packet_count); // Click-style name
    add_data_handlers("drops", Handler::OP_READ, &_drops);
};

/******************************************************************************************************/

CLICK_ENDDECLS

EXPORT_ELEMENT(LISPDecap)
ELEMENT_MT_SAFE(LISPDecap)

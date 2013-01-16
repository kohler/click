// -*- c-basic-offset: 4 -*-
/*
 * icmppingencap.{cc,hh} -- Encapsulate packets in ICMP ping headers.
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (C) 2003 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
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
#include "icmppingencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

ICMPPingEncap::ICMPPingEncap()
    : _icmp_id(0), _ip_id(1)
{
}

ICMPPingEncap::~ICMPPingEncap()
{
}

int
ICMPPingEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_mp("SRC", _src)
	.read_mp("DST", _dst)
	.read("IDENTIFIER", _icmp_id)
	.complete() < 0)
	return -1;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    { // check alignment
	int ans, c, o;
	ans = AlignmentInfo::query(this, 0, c, o);
	_aligned = (ans && c == 4 && o == 0);
	if (!_aligned)
	    errh->warning("IP header unaligned, cannot use fast IP checksum");
	if (!ans)
	    errh->message("(Try passing the configuration through `click-align'.)");
    }
#endif

    return 0;
}

Packet *
ICMPPingEncap::simple_action(Packet *p)
{
    if (WritablePacket *q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_echo))) {
	click_ip *ip = reinterpret_cast<click_ip *>(q->data());
	ip->ip_v = 4;
	ip->ip_hl = sizeof(click_ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_len = htons(q->length());
	ip->ip_id = htons(_ip_id);
	ip->ip_off = 0;
	ip->ip_ttl = 255;
	ip->ip_p = IP_PROTO_ICMP; /* icmp */
	ip->ip_sum = 0;
	ip->ip_src = _src;
	ip->ip_dst = _dst;

	click_icmp_echo *icmp = (struct click_icmp_echo *) (ip + 1);
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_identifier = htons(_icmp_id);
	icmp->icmp_sequence = htons(_ip_id);

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
	if (_aligned)
	    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
	else
	    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
	ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
	ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif
	icmp->icmp_cksum = click_in_cksum((const unsigned char *)icmp, q->length() - sizeof(click_ip));

	q->set_dst_ip_anno(IPAddress(_dst));
	q->set_ip_header(ip, sizeof(click_ip));

	_ip_id += (_ip_id == 0xFFFF ? 2 : 1);
	return q;
    } else
	return 0;
}

String ICMPPingEncap::read_handler(Element *e, void *thunk)
{
    ICMPPingEncap *i = static_cast<ICMPPingEncap *>(e);
    if (thunk)
	return IPAddress(i->_dst).unparse();
    else
	return IPAddress(i->_src).unparse();
}

int ICMPPingEncap::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    ICMPPingEncap *i = static_cast<ICMPPingEncap *>(e);
    IPAddress a;
    if (!IPAddressArg().parse(str, a))
	return errh->error("syntax error");
    if (thunk)
	i->_dst = a;
    else
	i->_src = a;
    return 0;
}

void ICMPPingEncap::add_handlers()
{
    add_read_handler("src", read_handler, 0, Handler::CALM);
    add_write_handler("src", write_handler, 0);
    add_read_handler("dst", read_handler, 1, Handler::CALM);
    add_write_handler("dst", write_handler, 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPPingEncap)

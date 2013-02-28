/*
 * udpip6encap.{cc,hh} -- element encapsulates packet in UDP/IP6 header
 * Wim Vandenberghe
 *
 * Copyright (c) 2009 Ghent University
 *
 * Based on udpipencap.{cc,hh}
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <clicknet/ip6.h>
#include "udpip6encap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/ip6address.hh>
CLICK_DECLS

UDPIP6Encap::UDPIP6Encap()
    : _use_dst_anno(false)
{
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    _checked_aligned = false;
#endif
}

UDPIP6Encap::~UDPIP6Encap()
{
}

int
UDPIP6Encap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IP6Address saddr;
    uint16_t sport, dport;
    String daddr_str;

    if (Args(conf, this, errh)
	.read_mp("SRC", saddr)
	.read_mp("SPORT", IPPortArg(IP_PROTO_UDP), sport)
	.read_mp("DST", AnyArg(), daddr_str)
	.read_mp("DPORT", IPPortArg(IP_PROTO_UDP), dport)
	.complete() < 0)
	return -1;

    if (daddr_str.equals("DST_ANNO", 8)) {
	_daddr = IP6Address();
	_use_dst_anno = true;
    } else if (cp_ip6_address(daddr_str, &_daddr, this))
	_use_dst_anno = false;
    else
	return errh->error("bad DST");

    _saddr = saddr;
    _sport = htons(sport);
    _dport = htons(dport);

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    if (!_checked_aligned) {
	int ans, c, o;
	ans = AlignmentInfo::query(this, 0, c, o);
	_aligned = (ans && c == 4 && o == 0);
	if (!_aligned)
	    errh->warning("IP header unaligned, cannot use fast IP checksum");
	if (!ans)
	    errh->message("(Try passing the configuration through %<click-align%>.)");
	_checked_aligned = true;
    }
#endif

    return 0;
}

Packet *
UDPIP6Encap::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip6));
    click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(p->data());
    click_udp *udp = reinterpret_cast<click_udp *>(ip6 + 1);

#if !HAVE_INDIFFERENT_ALIGNMENT
    assert((uintptr_t) ip6 % 4 == 0);
#endif

    // set up IP6 header
    ip6->ip6_flow = 0;	// set flow variable to 0 (includes version, traffic class and flow label)
    ip6->ip6_v = 6;		// then set version to 6
    uint16_t plen = htons(p->length() - sizeof(click_ip6));
    ip6->ip6_plen = plen;
    ip6->ip6_nxt = IP_PROTO_UDP;
    ip6->ip6_hlim = 0xff;
    ip6->ip6_src = _saddr;
    if (_use_dst_anno)
	ip6->ip6_dst = DST_IP6_ANNO(p);
    else {
	ip6->ip6_dst = _daddr;
	SET_DST_IP6_ANNO(p,_daddr);
    }
    p->set_ip6_header(ip6, sizeof(click_ip6));

    // set up UDP header
    udp->uh_sport = _sport;
    udp->uh_dport = _dport;
    udp->uh_ulen = plen;
    udp->uh_sum = 0;
    //TO DO: ADD SUPPORT FOR CORRECT CHECKSUM IN CASE OF ROUTING HEADER
    udp->uh_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, udp->uh_ulen, ip6->ip6_nxt, udp->uh_sum, (unsigned char *)(udp), udp->uh_ulen)); //reuse the icmp6 checksum calculation from ip6ndsolicitor.cc, because ICMPv6 uses the same checksum as UDP over IPv6 (see RFC 2460, 8.1)

    return p;
}

String UDPIP6Encap::read_handler(Element *e, void *thunk)
{
    UDPIP6Encap *u = static_cast<UDPIP6Encap *>(e);
    switch ((uintptr_t) thunk) {
      case 0:
	return IP6Address(u->_saddr).unparse();
      case 1:
	return String(ntohs(u->_sport));
      case 2:
	return IP6Address(u->_daddr).unparse();
      case 3:
	return String(ntohs(u->_dport));
      default:
	return String();
    }
}

void UDPIP6Encap::add_handlers()
{
    add_read_handler("src", read_handler, 0);
    add_write_handler("src", reconfigure_keyword_handler, "0 SRC");
    add_read_handler("sport", read_handler, 1);
    add_write_handler("sport", reconfigure_keyword_handler, "1 SPORT");
    add_read_handler("dst", read_handler, 2);
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
    add_read_handler("dport", read_handler, 3);
    add_write_handler("dport", reconfigure_keyword_handler, "3 DPORT");
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ip6)
EXPORT_ELEMENT(UDPIP6Encap)
ELEMENT_MT_SAFE(UDPIP6Encap)

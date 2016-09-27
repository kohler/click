/*
 * udpipencap.{cc,hh} -- element encapsulates packet in UDP/IP header
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
#include <clicknet/ip.h>
#include "udpipencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

UDPIPEncap::UDPIPEncap()
    : _cksum(true), _use_dst_anno(false)
{
    _id = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    _checked_aligned = false;
#endif
}

UDPIPEncap::~UDPIPEncap()
{
}

int
UDPIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPAddress saddr;
    uint16_t sport, dport;
    String daddr_str;
    bool cksum;

    if (Args(conf, this, errh)
	.read_mp("SRC", saddr)
	.read_mp("SPORT", IPPortArg(IP_PROTO_UDP), sport)
	.read_mp("DST", AnyArg(), daddr_str)
	.read_mp("DPORT", IPPortArg(IP_PROTO_UDP), dport)
	.read_p("CHECKSUM", BoolArg(), cksum)
	.complete() < 0)
	return -1;

    if (daddr_str.equals("DST_ANNO", 8)) {
	_daddr = IPAddress();
	_use_dst_anno = true;
    } else if (IPAddressArg().parse(daddr_str, _daddr, this))
	_use_dst_anno = false;
    else
	return errh->error("bad DST");

    _saddr = saddr;
    _sport = htons(sport);
    _dport = htons(dport);
    _cksum = cksum;

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
UDPIPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

#if !HAVE_INDIFFERENT_ALIGNMENT
  assert((uintptr_t)ip % 4 == 0);
#endif
  // set up IP header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.fetch_and_add(1));
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = _saddr;
  if (_use_dst_anno)
      ip->ip_dst = p->dst_ip_anno();
  else {
      ip->ip_dst = _daddr;
      p->set_dst_ip_anno(IPAddress(_daddr));
  }
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
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

  p->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = _sport;
  udp->uh_dport = _dport;
  uint16_t len = p->length() - sizeof(click_ip);
  udp->uh_ulen = htons(len);
  udp->uh_sum = 0;
  if (_cksum) {
    unsigned csum = click_in_cksum((unsigned char *)udp, len);
    udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
  }

  return p;
}

String UDPIPEncap::read_handler(Element *e, void *thunk)
{
    UDPIPEncap *u = static_cast<UDPIPEncap *>(e);
    switch ((uintptr_t) thunk) {
      case 0:
	return IPAddress(u->_saddr).unparse();
      case 1:
	return String(ntohs(u->_sport));
      case 2:
	return IPAddress(u->_daddr).unparse();
      case 3:
	return String(ntohs(u->_dport));
      default:
	return String();
    }
}

void UDPIPEncap::add_handlers()
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
EXPORT_ELEMENT(UDPIPEncap)
ELEMENT_MT_SAFE(UDPIPEncap)

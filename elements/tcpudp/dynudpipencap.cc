/*
 * dynudpipencap.{cc,hh} -- element encapsulates packet in UDP/IP header
 * Benjie Chen, Eddie Kohler
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
#include <clicknet/ip.h>
#include "dynudpipencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

DynamicUDPIPEncap::DynamicUDPIPEncap()
{
}

DynamicUDPIPEncap::~DynamicUDPIPEncap()
{
}

int
DynamicUDPIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool do_cksum = true;
    _interval = 0;
    if (Args(conf, this, errh)
	.read_mp("SRC", _saddr)
	.read_mp("SPORT", IPPortArg(IP_PROTO_UDP), _sport)
	.read_mp("DST", _daddr)
	.read_mp("DPORT", IPPortArg(IP_PROTO_UDP), _dport)
	.read_p("CHECKSUM", do_cksum)
	.read_p("INTERVAL", _interval)
	.complete() < 0)
	return -1;

  _id = 0;
  _cksum = do_cksum;
  _count = 0;

#ifdef CLICK_LINUXMODULE
  // check alignment
  {
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
DynamicUDPIPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

  // set up IP header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.fetch_and_add(1));
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = _saddr;
  ip->ip_dst = _daddr;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
#ifdef CLICK_LINUXMODULE
  if (_aligned) {
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  } else {
#endif
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#ifdef CLICK_LINUXMODULE
  }
#endif

  p->set_dst_ip_anno(IPAddress(_daddr));
  p->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  unsigned short len = p->length() - sizeof(click_ip);
  udp->uh_ulen = htons(len);
  udp->uh_sum = 0;
  if (_cksum) {
    unsigned csum = click_in_cksum((unsigned char *)udp, len);
    udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
  }

  unsigned old_count = _count.fetch_and_add(1);
  if (old_count == _interval-1 && _interval > 0) {
    _sport ++;
    _dport ++;
    _count = 0;
  }
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DynamicUDPIPEncap)
ELEMENT_MT_SAFE(UDPIPEncap)

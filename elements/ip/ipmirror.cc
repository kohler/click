/*
 * ipmirror.{cc,hh} -- rewrites IP packet a->b to b->a
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "ipmirror.hh"
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
CLICK_DECLS

IPMirror::IPMirror()
{
}

IPMirror::~IPMirror()
{
}

int
IPMirror::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _dst_anno = true;
    return Args(conf, this, errh).read_p("DST_ANNO", _dst_anno).complete();
}

Packet *
IPMirror::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  // new checksum is same as old checksum

  click_ip *iph = p->ip_header();
  struct in_addr tmpa = iph->ip_src;
  iph->ip_src = iph->ip_dst;
  iph->ip_dst = tmpa;
  if (_dst_anno)
      p->set_dst_ip_anno(tmpa);

  // may mirror ports as well
  if ((iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP) && IP_FIRSTFRAG(iph) && (int)p->length() >= p->transport_header_offset() + 8) {
    click_udp *udph = p->udp_header();
    uint16_t tmpp = udph->uh_sport;
    udph->uh_sport = udph->uh_dport;
    udph->uh_dport = tmpp;
    if (iph->ip_p == IP_PROTO_TCP) {
      click_tcp *tcph = p->tcp_header();
      uint32_t seqn = tcph->th_seq;
      tcph->th_seq = tcph->th_ack;
      tcph->th_ack = seqn;
    }
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPMirror)
ELEMENT_MT_SAFE(IPMirror)

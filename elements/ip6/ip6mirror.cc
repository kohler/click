/*
 * ip6mirror.{cc,hh} -- rewrites IP6 packet a->b to b->a
 * Max Poletto, Eddie Kohler, Peilei Fan
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "ip6mirror.hh"
#include <click/click_ip.h>
#include <click/click_ip6.h>
#include <click/click_udp.h>

IP6Mirror::IP6Mirror()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

IP6Mirror::~IP6Mirror()
{
  MOD_DEC_USE_COUNT;
}

Packet *
IP6Mirror::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  // new checksum is same as old checksum
  
  click_ip6 *iph = p->ip6_header();
  struct click_in6_addr tmpa = iph->ip6_src;
  iph->ip6_src = iph->ip6_dst;
  iph->ip6_dst = tmpa;
  
  // may mirror ports as well
  if ((iph->ip6_nxt == IP_PROTO_TCP || iph->ip6_nxt == IP_PROTO_UDP) &&  p->length() >= p->transport_header_offset() + 8) {
    click_udp *udph = reinterpret_cast<click_udp *>(p->transport_header());
    unsigned short tmpp = udph->uh_sport;
    udph->uh_sport = udph->uh_dport;
    udph->uh_dport = tmpp;
  }
  
  return p;
}

EXPORT_ELEMENT(IP6Mirror)
ELEMENT_MT_SAFE(IP6Mirror)

/*
 * ip6mirror.{cc,hh} -- rewrites IP6 packet a->b to b->a
 * Max Poletto, Eddie Kohler, Peilei Fan
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
#include "ip6mirror.hh"
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/udp.h>
CLICK_DECLS

IP6Mirror::IP6Mirror()
{
}

IP6Mirror::~IP6Mirror()
{
}

Packet *
IP6Mirror::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  // new checksum is same as old checksum

  click_ip6 *iph = p->ip6_header();
  struct in6_addr tmpa = iph->ip6_src;
  iph->ip6_src = iph->ip6_dst;
  iph->ip6_dst = tmpa;

  // may mirror ports as well
  if ((iph->ip6_nxt == IP_PROTO_TCP || iph->ip6_nxt == IP_PROTO_UDP) &&  p->length() >= (uint32_t)(p->transport_header_offset() + 8)) {
    click_udp *udph = reinterpret_cast<click_udp *>(p->transport_header());
    uint16_t tmpp = udph->uh_sport;
    udph->uh_sport = udph->uh_dport;
    udph->uh_dport = tmpp;
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IP6Mirror)
ELEMENT_MT_SAFE(IP6Mirror)

/*
 * ipmirror.{cc,hh} -- rewrites IP packet a->b to b->a
 * Max Poletto, Eddie Kohler
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipmirror.hh"
#include <click/click_ip.h>
#include <click/click_udp.h>

Packet *
IPMirror::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  // new checksum is same as old checksum
  
  click_ip *iph = p->ip_header();
  struct in_addr tmpa = iph->ip_src;
  iph->ip_src = iph->ip_dst;
  iph->ip_dst = tmpa;
  
  click_udp *udph = reinterpret_cast<click_udp *>(p->transport_header());
  unsigned short tmpp = udph->uh_sport;
  udph->uh_sport = udph->uh_dport;
  udph->uh_dport = tmpp;
  
  return p;
}

EXPORT_ELEMENT(IPMirror)

/*
 * ipmirror.{cc,hh} -- rewrites IP packet a->b to b->a
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipmirror.hh"
#include "click_ip.h"
#include "click_udp.h"

Packet *
IPMirror::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  // new checksum is same as old checksum
  
  click_ip *iph = p->ip_header();
  struct in_addr tmpa = iph->ip_src;
  iph->ip_src = iph->ip_dst;
  iph->ip_dst = tmpa;
  
  click_udp *udph = (click_udp *)p->transport_header();
  unsigned short tmpp = udph->uh_sport;
  udph->uh_sport = udph->uh_dport;
  udph->uh_dport = tmpp;
  
  return p;
}

EXPORT_ELEMENT(IPMirror)

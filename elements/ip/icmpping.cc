/*
 * icmpping.{cc,hh} -- element constructs ICMP echo response packets
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/config.h>
#include <click/package.hh>
#include "icmpping.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_ether.h>
#include <click/click_ip.h>
#include <click/click_icmp.h>

ICMPPing::ICMPPing()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
}

ICMPPing::~ICMPPing()
{
  MOD_DEC_USE_COUNT;
}


ICMPPing *
ICMPPing::clone() const
{
  return new ICMPPing;
}


Packet *
ICMPPing::make_echo_response(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ether *eth_header = reinterpret_cast<click_ether *>(p->data());
  click_ip *ip_header = reinterpret_cast<click_ip *>(eth_header + 1);
  icmp_echo *icmp = reinterpret_cast<icmp_echo *>(ip_header + 1);
  // XXX IP options
  unsigned len = ntohs(ip_header->ip_len) - (ip_header->ip_hl << 2);

  /* him */
  u_char tha[6]; 
  u_char tpa[4];
  memcpy(tha, eth_header->ether_shost, 6);
  memcpy(tpa, &(ip_header->ip_src.s_addr), 4);
  
  /* me */
  u_char sha[6];
  u_char spa[4];
  memcpy(sha, eth_header->ether_dhost, 6);
  memcpy(spa, &(ip_header->ip_dst.s_addr), 4);
  
  /* swap src and target ip and host addresses */
  memcpy(eth_header->ether_dhost,tha,6);
  memcpy(eth_header->ether_shost,sha,6);
  memcpy(&(ip_header->ip_dst.s_addr),tpa,4);
  memcpy(&(ip_header->ip_src.s_addr),spa,4);

  /* set ICMP packet type to ICMP_ECHO_REPLY */
  icmp->icmp_type = ICMP_ECHO_REPLY;
  
  /* calculate ICMP checksum */
  icmp->icmp_cksum = in_cksum((unsigned char *)icmp, len);

  int pktlen = sizeof(click_ether) + ntohs(ip_header->ip_len);
  p->take(p->length()-pktlen);
  return p;
}


Packet *
ICMPPing::simple_action(Packet *p)
{
  const click_ether *eth_header =
    reinterpret_cast<const click_ether *>(p->data());
  const click_ip *ip_header =
    reinterpret_cast<const click_ip *>(eth_header + 1);
  const icmp_generic *icmp =
    reinterpret_cast<const icmp_generic *>(ip_header + 1);
  // XXX IP options

  if (ip_header->ip_p != IP_PROTO_ICMP) {
    click_chatter("icmpresponder: packet not an ICMP packet");
    p->kill();
    return 0;
  }

  switch (icmp->icmp_type) {
    
   case ICMP_ECHO:
    return make_echo_response(p);
    
   default:
    click_chatter("icmpresponder: packet not an ICMP-echo packet");
    p->kill();
    return 0;
    
  }
}

EXPORT_ELEMENT(ICMPPing)

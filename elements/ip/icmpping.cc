/*
 * icmpping.{cc,hh} -- element constructs ICMP echo response packets
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "icmpping.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "click_ether.h"
#include "click_ip.h"
#include "click_icmp.h"

ICMPPing::ICMPPing()
  : Element(1,1)
{
}


ICMPPing *
ICMPPing::clone() const
{
  return new ICMPPing;
}


void
ICMPPing::make_echo_response(Packet *p)
{
  struct ether_header *eth_header = (struct ether_header *) p->data();
  click_ip *ip_header = (click_ip *) (eth_header+1);
  struct icmp_echo *icmp = (struct icmp_echo *) (ip_header+1);

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
  icmp->icmp_cksum = in_cksum((unsigned char *)icmp, sizeof(struct icmp_echo));

  int pktlen = sizeof(struct ether_header)+ntohs(ip_header->ip_len);
  p->take(p->length()-pktlen);
}


Packet *
ICMPPing::simple_action(Packet *p)
{
  struct ether_header *eth_header = (struct ether_header *) p->data();
  click_ip *ip_header = (click_ip *) (eth_header+1);
  struct icmp_generic *icmp = (struct icmp_generic *) (ip_header+1);

  if (ip_header->ip_p != IP_PROTO_ICMP)
  {
    click_chatter("icmpresponder: packet not an ICMP packet");
    p->kill();
    return 0L;
  }

  switch(icmp->icmp_type)
  {
    case ICMP_ECHO:
      make_echo_response(p);
      return p;
    default:
      click_chatter("icmpresponder: packet not an ICMP-echo packet");
      p->kill();
      return 0L;
  }
}

EXPORT_ELEMENT(ICMPPing)

/*
 * icmppingresponder.{cc,hh} -- element constructs ICMP echo response packets
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

#include <click/config.h>
#include <click/package.hh>
#include "icmppingresponder.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_ether.h>
#include <click/click_ip.h>
#include <click/click_icmp.h>

ICMPPingResponder::ICMPPingResponder()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
}

ICMPPingResponder::~ICMPPingResponder()
{
  MOD_DEC_USE_COUNT;
}


ICMPPingResponder *
ICMPPingResponder::clone() const
{
  return new ICMPPingResponder;
}

Packet *
ICMPPingResponder::simple_action(Packet *p_in)
{
  const click_ip *iph_in = p_in->ip_header();
  const icmp_generic *icmph_in = reinterpret_cast<const icmp_generic *>(p_in->transport_header());

  if (iph_in->ip_p != IP_PROTO_ICMP || icmph_in->icmp_type != ICMP_ECHO)
    return p_in;

  WritablePacket *q = p_in->uniqueify();
  
  // swap src and target ip addresses (checksum remains valid)
  click_ip *iph = q->ip_header();
  struct in_addr tmp_addr = iph->ip_dst;
  iph->ip_dst = iph->ip_src;
  iph->ip_src = tmp_addr;
  
  // set ICMP packet type to ICMP_ECHO_REPLY and recalculate checksum
  icmp_sequenced *icmph = reinterpret_cast<icmp_sequenced *>(q->transport_header());
  unsigned short old_hw = ((unsigned short *)icmph)[0];
  icmph->icmp_type = ICMP_ECHO_REPLY;
  icmph->icmp_code = 0;
  unsigned short new_hw = ((unsigned short *)icmph)[0];
  
  // incrementally update IP checksum according to RFC1624:
  // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
  unsigned sum = (~icmph->icmp_cksum & 0xFFFF) + (~old_hw & 0xFFFF) + new_hw;
  sum = (sum & 0xFFFF) + (sum >> 16);
  icmph->icmp_cksum = ~(sum + (sum >> 16));

  return q;
}

EXPORT_ELEMENT(ICMPPingResponder)
ELEMENT_MT_SAFE(ICMPPingResponder)

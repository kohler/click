/*
 * ForceIP.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "forceip.hh"
#include "error.hh"
#include "glue.hh"
#include "elements/standard/alignmentinfo.hh"

ForceIP::ForceIP()
  : Element(1, 1)
{
  _count = 0;
}

ForceIP::~ForceIP()
{
}

ForceIP *
ForceIP::clone() const
{
  return new ForceIP;
}

Packet *
ForceIP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  unsigned plen = p->length();

  ip->ip_v = 4;
  ip->ip_len = htons(plen);

  if((_count++ & 7) != 1){
    ip->ip_off = 0;
  }

  unsigned hlen = ip->ip_hl << 2;
  if(hlen < sizeof(click_ip) || hlen > plen){
    ip->ip_hl = plen >> 2;
  }

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, ip->ip_hl << 2);
  
  return p;
}

EXPORT_ELEMENT(ForceIP)

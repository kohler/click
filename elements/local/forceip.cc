/*
 * ForceIP.{cc,hh} -- element encapsulates packet in IP header
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
#include "forceip.hh"
#include <click/error.hh>
#include <click/glue.hh>
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

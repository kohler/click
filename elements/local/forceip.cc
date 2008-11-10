/*
 * ForceIP.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "forceip.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

ForceIP::ForceIP()
{
  _count = 0;
}

ForceIP::~ForceIP()
{
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
  ip->ip_sum = click_in_cksum((unsigned char *)ip, ip->ip_hl << 2);

  p->set_ip_header(ip, hlen);

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ForceIP)

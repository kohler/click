/*
 * setipchecksum.{cc,hh} -- element sets IP header checksum
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
#include "setipchecksum.hh"
#include <click/glue.hh>
#include <click/click_ip.h>

SetIPChecksum::SetIPChecksum()
{
  add_input();
  add_output();
}

SetIPChecksum::~SetIPChecksum()
{
}

SetIPChecksum *
SetIPChecksum::clone() const
{
  return new SetIPChecksum();
}

Packet *
SetIPChecksum::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->length() - p->ip_header_offset();
  unsigned hlen;
  
  if (!ip || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  p = p->uniqueify();
  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, hlen);
  return p;

 bad:
  click_chatter("SetIPChecksum: bad lengths");
  p->kill();
  return(0);
}

EXPORT_ELEMENT(SetIPChecksum)

/*
 * setipchecksum.{cc,hh} -- element sets IP header checksum
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
#include "setipchecksum.hh"
#include "glue.hh"
#include "click_ip.h"

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
SetIPChecksum::simple_action(Packet *p)
{
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

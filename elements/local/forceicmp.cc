/*
 * ForceICMP.{cc,hh} -- sets the ICMP header checksum
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
#include "forceicmp.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/click_icmp.h>
#include <click/confparse.hh>

ForceICMP::ForceICMP()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _count = 0;
  _type = -1;
  _code = -1;
}

ForceICMP::~ForceICMP()
{
  MOD_DEC_USE_COUNT;
}

int
ForceICMP::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int ret;

  ret = cp_va_parse(conf, this, errh,
                    cpOptional,
                    cpInteger, "ICMP type", &_type,
                    cpInteger, "ICMP code", &_code,
                    0);

  return(ret);
}

ForceICMP *
ForceICMP::clone() const
{
  return new ForceICMP();
}

Packet *
ForceICMP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->length() - p->ip_header_offset();
  unsigned hlen, ilen;
  icmp_generic *ih;

  if (!ip || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  ilen = ntohs(ip->ip_len);
  if(ilen > plen || ilen < hlen + sizeof(icmp_generic))
    goto bad;

  ih = (icmp_generic *) (((char *)ip) + hlen);

  if(random() & 1){
    ih->icmp_type = random() % 20;
    ih->icmp_code = random() % 20;
  }

  if(_type >= 0)
    ih->icmp_type = _type;
  if(_code >= 0)
    ih->icmp_code = _code;

  _count++;

  ih->icmp_cksum = 0;
  ih->icmp_cksum = in_cksum((unsigned char *)ih, ilen - hlen);

  return p;

 bad:
  click_chatter("ForceICMP: bad lengths");
  p->kill();
  return(0);
}

EXPORT_ELEMENT(ForceICMP)

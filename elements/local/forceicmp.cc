/*
 * ForceICMP.{cc,hh} -- sets the ICMP header checksum
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
#include "forceicmp.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/confparse.hh>
CLICK_DECLS

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
ForceICMP::configure(Vector<String> &conf, ErrorHandler *errh)
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
  click_icmp *ih;

  if (!ip || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  ilen = ntohs(ip->ip_len);
  if(ilen > plen || ilen < hlen + sizeof(click_icmp))
    goto bad;

  ih = (click_icmp *) (((char *)ip) + hlen);

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
  ih->icmp_cksum = click_in_cksum((unsigned char *)ih, ilen - hlen);

  return p;

 bad:
  click_chatter("ForceICMP: bad lengths");
  p->kill();
  return(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ForceICMP)

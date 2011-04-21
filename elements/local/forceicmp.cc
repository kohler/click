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
#include <click/args.hh>
#include <click/nameinfo.hh>
CLICK_DECLS

ForceICMP::ForceICMP()
{
  _count = 0;
  _type = -1;
  _code = -1;
}

ForceICMP::~ForceICMP()
{
}

int
ForceICMP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String code_str;
    if (Args(conf, this, errh)
	.read_mp("TYPE", NamedIntArg(NameInfo::T_ICMP_TYPE), _type)
	.read_mp("CODE", WordArg(), code_str)
	.complete() < 0)
	return -1;
    if (_type < 0 || _type > 255)
	return errh->error("ICMP type must be between 0 and 255");
    if (!NameInfo::query_int(NameInfo::T_ICMP_CODE + _type, this, code_str, &_code)
	|| _code < 0 || _code > 255)
	return errh->error("argument 2 takes ICMP code");
    return 0;
}

Packet *
ForceICMP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->network_length();
  unsigned hlen, ilen;
  click_icmp *ih;

  if (!p->has_network_header() || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  ilen = ntohs(ip->ip_len);
  if(ilen > plen || ilen < hlen + sizeof(click_icmp))
    goto bad;

  ih = (click_icmp *) (((char *)ip) + hlen);

  if (click_random(0, 1)) {
      ih->icmp_type = click_random(0, 19);
      ih->icmp_code = click_random(0, 19);
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

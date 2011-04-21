/*
 * ForceUDP.{cc,hh} -- sets the UDP header checksum
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
#include "forceudp.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/args.hh>
CLICK_DECLS

ForceUDP::ForceUDP()
{
  _count = 0;
  _dport = -1;
}

ForceUDP::~ForceUDP()
{
}

int
ForceUDP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() == 0 || conf[0] == "-1")
	return 0;
    uint16_t dp;
    if (Args(conf, this, errh)
	.read_p("DPORT", IPPortArg(IP_PROTO_UDP), dp)
	.complete() < 0)
	return -1;
    _dport = dp;
    return 0;
}

Packet *
ForceUDP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->network_length();
  unsigned hlen, ilen, oisum;
  char itmp[9];
  click_udp *uh;

  if (!p->has_network_header() || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  ilen = ntohs(ip->ip_len);
  if(/*ilen > plen || */ilen < hlen + sizeof(click_udp))
    goto bad;

  uh = (click_udp *) (((char *)ip) + hlen);

  uh->uh_ulen = htons(ilen - hlen);

  if(_dport >= 0){
    uh->uh_dport = htons(_dport);
  } else {
    uh->uh_dport = htons(click_random(0, 1023));
  }
  _count++;

  memcpy(itmp, ip, 9);
  memset(ip, '\0', 9);
  oisum = ip->ip_sum;
  ip->ip_sum = 0;
  ip->ip_len = htons(ilen - hlen);

  uh->uh_sum = 0;
  uh->uh_sum = click_in_cksum((unsigned char *)ip, ilen);

  memcpy(ip, itmp, 9);
  ip->ip_sum = oisum;
  ip->ip_len = htons(ilen);

  return p;

 bad:
  click_chatter("ForceUDP: bad lengths");
  p->kill();
  return(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ForceUDP)

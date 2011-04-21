/*
 * ForceTCP.{cc,hh} -- sets the TCP header checksum
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
#include "forcetcp.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/args.hh>
CLICK_DECLS

ForceTCP::ForceTCP()
{
  _count = 0;
  _random = false;
  _flags = -1;
}

ForceTCP::~ForceTCP()
{
}

int
ForceTCP::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _dport = 0;
  return Args(conf, this, errh)
      .read_p("DPORT", IPPortArg(IP_PROTO_TCP), _dport)
      .read_p("RANDOM_DPORT", _random)
      .read_p("FLAGS", _flags)
      .complete();
}

Packet *
ForceTCP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->network_length();
  unsigned hlen, ilen, oisum, off;
  char itmp[9];
  click_tcp *th;

  if (!p->has_network_header() || plen < sizeof(click_ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto bad;

  ilen = ntohs(ip->ip_len);
  if(ilen > plen || ilen < hlen + sizeof(click_tcp))
    goto bad;

  th = (click_tcp *) (((char *)ip) + hlen);

  off = th->th_off << 2;
  if(off < sizeof(click_tcp) || off > (ilen - hlen)){
    int noff;
#if 1
    if(ilen - hlen - sizeof(click_tcp) > 0){
      noff = click_random(0, ilen - hlen - sizeof(click_tcp) - 1);
    } else {
      noff = ilen - hlen;
    }
#else
    noff = sizeof(click_tcp);
#endif
    th->th_off = noff >> 2;
  }

  if(_flags != -1){
    th->th_flags = _flags;
  }

  if(_dport > 0){
    th->th_dport = htons(_dport);
  }
  else if (_random) {
#if 1
    if((_count & 7) < 2){
      th->th_dport = htons(80);
    } else if((_count & 7) == 3){
      th->th_dport = htons(click_random() % 1024);
    }
#else
    th->th_dport = htons(click_random() % 1024);
#endif
  }
  _count++;

  memcpy(itmp, ip, 9);
  memset(ip, '\0', 9);
  oisum = ip->ip_sum;
  ip->ip_sum = 0;
  ip->ip_len = htons(ilen - hlen);

  th->th_sum = 0;
  th->th_sum = click_in_cksum((unsigned char *)ip, ilen);

  memcpy(ip, itmp, 9);
  ip->ip_sum = oisum;
  ip->ip_len = htons(ilen);

  return p;

 bad:
  click_chatter("ForceTCP: bad lengths");
  p->kill();
  return(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ForceTCP)

/*
 * TCPReflector.{cc,hh} -- toy TCP server
 * Robert Morris
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "tcpreflector.hh"
#include <clicknet/tcp.h>
#include <clicknet/ip.h>
#include <click/ipaddress.hh>
#include <click/glue.hh>
CLICK_DECLS

TCPReflector::TCPReflector()
{
}

TCPReflector::~TCPReflector()
{
}

Packet *
TCPReflector::tcp_input(Packet *xp)
{
  WritablePacket *p = xp->uniqueify();
  unsigned seq, ack, off, hlen;
  unsigned plen = p->length();
  struct in_addr src, dst;
  unsigned short sport, dport;
  char itmp[9];
  int dlen;
  click_ip *ip;
  click_tcp *th;

  if(plen < sizeof(click_ip) + sizeof(click_tcp))
    goto ignore;

  ip = (click_ip *) p->data();
  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > plen)
    goto ignore;

  th = (click_tcp *) (((char *)ip) + hlen);
  off = th->th_off << 2;
  dlen = plen - hlen - off;

  src = ip->ip_src;
  dst = ip->ip_dst;
  sport = th->th_sport;
  dport = th->th_dport;
  seq = ntohl(th->th_seq);
  ack = ntohl(th->th_ack);

  th->th_flags &= ~TH_PUSH;

  if(th->th_flags == TH_SYN){
    th->th_flags = TH_SYN | TH_ACK;
    th->th_seq = click_random(0, 0xFFFFFFFFU);
    th->th_ack = htonl(seq + 1);
  } else if(th->th_flags & TH_SYN){
    goto ignore;
  } else if(th->th_flags & TH_RST){
    goto ignore;
  } else if(dlen > 0 || (th->th_flags & TH_FIN)){
    th->th_seq = htonl(ack);
    if(th->th_flags & TH_FIN){
      th->th_flags = TH_ACK | TH_FIN;
      th->th_ack = htonl(seq + dlen + 1);
    } else {
      th->th_flags = TH_ACK;
      th->th_ack = htonl(seq + dlen);
    }
  } else {
    goto ignore;
  }

  ip->ip_src = dst;
  ip->ip_dst = src;
  ip->ip_ttl = 250;
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));

  th->th_sport = dport;
  th->th_dport = sport;
  th->th_win = htons(60 * 1024);

  memcpy(itmp, ip, 9);
  memset(ip, '\0', 9);
  ip->ip_sum = 0;
  ip->ip_len = htons(plen - 20);

  th->th_sum = 0;
  th->th_sum = click_in_cksum((unsigned char *)ip, plen);

  memcpy(ip, itmp, 9);
  ip->ip_len = htons(plen);

  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  return(p);

 ignore:
  if(p)
    p->kill();
  return(0);
}

Packet *
TCPReflector::simple_action(Packet *p)
{
  return(tcp_input(p));
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPReflector)

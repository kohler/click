/*
 * ForceTCP.{cc,hh} -- sets the TCP header checksum
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
#include "forcetcp.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>

ForceTCP::ForceTCP()
{
  add_input();
  add_output();
  _count = 0;
}

ForceTCP::~ForceTCP()
{
}

ForceTCP *
ForceTCP::clone() const
{
  return new ForceTCP();
}

Packet *
ForceTCP::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->length() - p->ip_header_offset();
  unsigned hlen, ilen, oisum, off;
  char itmp[9];
  click_tcp *th;

  if (!ip || plen < sizeof(click_ip))
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
    if(ilen - hlen - sizeof(click_tcp) > 0){
      noff = random() % (ilen - hlen - sizeof(click_tcp));
    } else {
      noff = ilen - hlen;
    }
    th->th_off = noff >> 2;
  }

  if((_count & 7) < 2){
    th->th_dport = htons(80);
  } else if((_count & 7) == 3){
    th->th_dport = htons(random() % 1024);
  }
  _count++;

  memcpy(itmp, ip, 9);
  memset(ip, '\0', 9);
  oisum = ip->ip_sum;
  ip->ip_sum = 0;
  ip->ip_len = htons(ilen - hlen);

  th->th_sum = 0;
  th->th_sum = in_cksum((unsigned char *)ip, ilen);

  memcpy(ip, itmp, 9);
  ip->ip_sum = oisum;
  ip->ip_len = htons(ilen);

  return p;

 bad:
  click_chatter("ForceTCP: bad lengths");
  p->kill();
  return(0);
}

EXPORT_ELEMENT(ForceTCP)

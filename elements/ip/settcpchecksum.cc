/*
 * SetTCPChecksum.{cc,hh} -- sets the TCP header checksum
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
#include "settcpchecksum.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>

SetTCPChecksum::SetTCPChecksum()
{
  add_input();
  add_output();
  _fixoff = false;
}

SetTCPChecksum::~SetTCPChecksum()
{
}

int
SetTCPChecksum::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpBool, "fix th_off?", &_fixoff,
		  0) < 0)
    return -1;
  return 0;
}

SetTCPChecksum *
SetTCPChecksum::clone() const
{
  return new SetTCPChecksum();
}

Packet *
SetTCPChecksum::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned plen = p->length() - p->ip_header_offset();
  unsigned int hlen, ilen, oisum;
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

  if(_fixoff){
    unsigned int off = th->th_off << 2;
    if(off < sizeof(click_tcp) || off > (ilen - hlen)){
      th->th_off = (ilen - hlen) >> 2;
    }
  }    

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
  click_chatter("SetTCPChecksum: bad lengths");
  p->kill();
  return(0);
}

EXPORT_ELEMENT(SetTCPChecksum)

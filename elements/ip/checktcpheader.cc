/*
 * checktcpheader.{cc,hh} -- element checks TCP header for correctness
 * (checksums, lengths)
 * Eddie Kohler
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
#include "checktcpheader.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "bitvector.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckTCPHeader::CheckTCPHeader()
  : _drops(0)
{
  add_input();
  add_output();
}

CheckTCPHeader *
CheckTCPHeader::clone() const
{
  return new CheckTCPHeader();
}

void
CheckTCPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

Packet *
CheckTCPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  unsigned len, iph_len, tcph_len, csum;
  const click_tcp *tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  
  if (!iph || iph->ip_p != IP_PROTO_TCP)
    goto bad;

  iph_len = iph->ip_hl << 2;
  len = ntohs(iph->ip_len) - iph_len;
  tcph_len = tcph->th_off << 2;
  if (tcph_len < sizeof(click_tcp) || len < tcph_len
      || p->length() < len + iph_len + p->ip_header_offset())
    goto bad;

  csum = ~in_cksum((unsigned char *)tcph, len) & 0xFFFF;
#ifdef __KERNEL__
  csum = csum_tcpudp_magic(iph->ip_src.s_addr, iph->ip_dst.s_addr,
			   len, IP_PROTO_TCP, csum);
  if (csum != 0)
    goto bad;
#else
  {
    unsigned short *words = (unsigned short *)&iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_TCP);
    csum += htons(len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    if (csum != 0xFFFF)
      goto bad;
  }
#endif

  return p;
  
 bad:
  if (_drops == 0)
    click_chatter("TCP header check failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  
  return 0;
}

static String
CheckTCPHeader_read_drops(Element *thunk, void *)
{
  CheckTCPHeader *e = (CheckTCPHeader *)thunk;
  return String(e->drops()) + "\n";
}

void
CheckTCPHeader::add_handlers()
{
  add_read_handler("drops", CheckTCPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckTCPHeader)

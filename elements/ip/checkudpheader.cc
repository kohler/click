/*
 * checkudpheader.{cc,hh} -- element checks UDP header for correctness
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
#include "checkudpheader.hh"
#include <click/click_ip.h>
#include <click/click_udp.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckUDPHeader::CheckUDPHeader()
  : _drops(0)
{
  add_input();
  add_output();
}

CheckUDPHeader *
CheckUDPHeader::clone() const
{
  return new CheckUDPHeader();
}

void
CheckUDPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

Packet *
CheckUDPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  unsigned len, iph_len;
  const click_udp *udph =
    reinterpret_cast<const click_udp *>(p->transport_header());
  
  if (!iph || iph->ip_p != IP_PROTO_UDP)
    goto bad;

  iph_len = iph->ip_hl << 2;
  len = ntohs(udph->uh_ulen);
  if (len < sizeof(click_udp)
      || p->length() < len + iph_len + p->ip_header_offset())
    goto bad;

  if (udph->uh_sum != 0) {
    unsigned csum = ~in_cksum((unsigned char *)udph, len) & 0xFFFF;
#ifdef __KERNEL__
    if (csum_tcpudp_magic(iph->ip_src.s_addr, iph->ip_dst.s_addr,
			  len, IP_PROTO_UDP, csum) != 0)
      goto bad;
#else
    unsigned short *words = (unsigned short *)&iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_UDP);
    csum += htons(len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    if (csum != 0xFFFF)
      goto bad;
#endif
  }

  return p;
  
 bad:
  if (_drops == 0)
    click_chatter("UDP checksum failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  
  return 0;
}

static String
CheckUDPHeader_read_drops(Element *thunk, void *)
{
  CheckUDPHeader *e = (CheckUDPHeader *)thunk;
  return String(e->drops()) + "\n";
}

void
CheckUDPHeader::add_handlers()
{
  add_read_handler("drops", CheckUDPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckUDPHeader)

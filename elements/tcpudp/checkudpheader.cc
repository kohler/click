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

#include <click/config.h>
#include "checkudpheader.hh"
#include <click/click_ip.h>
#include <click/click_udp.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

const char *CheckUDPHeader::reason_texts[NREASONS] = {
  "not UDP", "bad packet length", "bad UDP checksum"
};

CheckUDPHeader::CheckUDPHeader()
  : Element(1, 1), _reason_drops(0)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

CheckUDPHeader::~CheckUDPHeader()
{
  MOD_DEC_USE_COUNT;
  delete[] _reason_drops;
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

int
CheckUDPHeader::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  bool verbose = false;
  bool details = false;
  
  if (cp_va_parse(conf, this, errh,
		  cpKeywords,
		  "VERBOSE", cpBool, "be verbose?", &verbose,
		  "DETAILS", cpBool, "keep detailed counts?", &details,
		  0) < 0)
    return -1;
  
  _verbose = verbose;
  if (details)
    _reason_drops = new u_atomic32_t[NREASONS];
  
  return 0;
}

Packet *
CheckUDPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("UDP header check failed: %s", reason_texts[reason]);
  _drops++;

  if (_reason_drops)
    _reason_drops[reason]++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();

  return 0;
}

Packet *
CheckUDPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  unsigned len, iph_len;
  const click_udp *udph =
    reinterpret_cast<const click_udp *>(p->transport_header());
  
  if (!iph || iph->ip_p != IP_PROTO_UDP)
    return drop(NOT_UDP, p);

  iph_len = iph->ip_hl << 2;
  len = ntohs(udph->uh_ulen);
  if (len < sizeof(click_udp)
      || p->length() < len + iph_len + p->ip_header_offset())
    return drop(BAD_LENGTH, p);

  if (udph->uh_sum != 0) {
    unsigned csum = ~in_cksum((unsigned char *)udph, len) & 0xFFFF;
#ifdef __KERNEL__
    if (csum_tcpudp_magic(iph->ip_src.s_addr, iph->ip_dst.s_addr,
			  len, IP_PROTO_UDP, csum) != 0)
      return drop(BAD_CHECKSUM, p);
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
      return drop(BAD_CHECKSUM, p);
#endif
  }

  return p;
}

String
CheckUDPHeader::read_handler(Element *e, void *thunk)
{
  CheckUDPHeader *c = reinterpret_cast<CheckUDPHeader *>(e);
  switch ((int)thunk) {

   case 0:			// drops
    return String(c->_drops) + "\n";

   case 1: {			// drop_details
     StringAccum sa;
     for (int i = 0; i < NREASONS; i++)
       sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
     return sa.take_string();
   }

   default:
    return String("<error>\n");

  }
}

void
CheckUDPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

EXPORT_ELEMENT(CheckUDPHeader)

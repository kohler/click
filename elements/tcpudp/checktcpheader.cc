/*
 * checktcpheader.{cc,hh} -- element checks TCP header for correctness
 * (checksums, lengths)
 * Eddie Kohler
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
#include "checktcpheader.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#ifdef CLICK_LINUXMODULE
# include <net/checksum.h>
#endif
CLICK_DECLS

const char *CheckTCPHeader::reason_texts[NREASONS] = {
  "not TCP", "bad packet length", "bad TCP checksum"
};

CheckTCPHeader::CheckTCPHeader()
  : Element(1, 1), _reason_drops(0)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

CheckTCPHeader::~CheckTCPHeader()
{
  MOD_DEC_USE_COUNT;
  delete[] _reason_drops;
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

int
CheckTCPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
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
    _reason_drops = new uatomic32_t[NREASONS];
  
  return 0;
}

Packet *
CheckTCPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("TCP header check failed: %s", reason_texts[reason]);
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
CheckTCPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  const click_tcp *tcph = p->tcp_header();
  unsigned len, iph_len, tcph_len, csum;
  
  if (!iph || iph->ip_p != IP_PROTO_TCP)
    return drop(NOT_TCP, p);

  iph_len = iph->ip_hl << 2;
  len = ntohs(iph->ip_len) - iph_len;
  tcph_len = tcph->th_off << 2;
  if (tcph_len < sizeof(click_tcp) || len < tcph_len
      || p->length() < len + iph_len + p->ip_header_offset())
    return drop(BAD_LENGTH, p);

  csum = ~click_in_cksum((unsigned char *)tcph, len) & 0xFFFF;
#ifdef CLICK_LINUXMODULE
  csum = csum_tcpudp_magic(iph->ip_src.s_addr, iph->ip_dst.s_addr,
			   len, IP_PROTO_TCP, csum);
  if (csum != 0)
    return drop(BAD_CHECKSUM, p);
#else
  {
    unsigned short *words = (unsigned short *)&iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_TCP);
    csum += htons(len);
    csum = (csum & 0xFFFF) + (csum >> 16);
    if (((csum + (csum >> 16)) & 0xFFFF) != 0xFFFF)
      return drop(BAD_CHECKSUM, p);
  }
#endif

  return p;
}

String
CheckTCPHeader::read_handler(Element *e, void *thunk)
{
  CheckTCPHeader *c = reinterpret_cast<CheckTCPHeader *>(e);
  switch ((intptr_t)thunk) {

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
CheckTCPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckTCPHeader)

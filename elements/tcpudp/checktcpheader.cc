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
#include <click/args.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
CLICK_DECLS

const char *CheckTCPHeader::reason_texts[NREASONS] = {
  "not TCP", "bad packet length", "bad TCP checksum"
};

CheckTCPHeader::CheckTCPHeader()
  : _reason_drops(0)
{
  _drops = 0;
}

CheckTCPHeader::~CheckTCPHeader()
{
  delete[] _reason_drops;
}

int
CheckTCPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool verbose = false;
    bool details = false;

    if (Args(conf, this, errh)
	.read("VERBOSE", verbose)
	.read("DETAILS", details)
	.complete() < 0)
	return -1;

  _verbose = verbose;
  if (details) {
    _reason_drops = new atomic_uint32_t[NREASONS];
    for (int i = 0; i < NREASONS; ++i)
      _reason_drops[i] = 0;
  }

  return 0;
}

Packet *
CheckTCPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("%p{element}: TCP header check failed: %s", this, reason_texts[reason]);
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

  if (!p->has_network_header() || iph->ip_p != IP_PROTO_TCP)
    return drop(NOT_TCP, p);

  iph_len = iph->ip_hl << 2;
  len = ntohs(iph->ip_len) - iph_len;
  tcph_len = tcph->th_off << 2;
  if (tcph_len < sizeof(click_tcp) || len < tcph_len
      || p->length() < len + iph_len + p->network_header_offset())
    return drop(BAD_LENGTH, p);

  csum = click_in_cksum((unsigned char *)tcph, len);
  if (click_in_cksum_pseudohdr(csum, iph, len) != 0)
    return drop(BAD_CHECKSUM, p);

  return p;
}

String
CheckTCPHeader::read_handler(Element *e, void *thunk)
{
  CheckTCPHeader *c = reinterpret_cast<CheckTCPHeader *>(e);
  switch ((intptr_t)thunk) {

   case 0:			// drops
    return String(c->_drops);

   case 1: {			// drop_details
     StringAccum sa;
     for (int i = 0; i < NREASONS; i++)
       sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
     return sa.take_string();
   }

   default:
    return String("<error>");

  }
}

void
CheckTCPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, 0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckTCPHeader)

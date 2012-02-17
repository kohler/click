/*
 * checkicmpheader.{cc,hh} -- element checks ICMP header for correctness
 * (checksums, lengths)
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "checkicmpheader.hh"
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
CLICK_DECLS

const char *CheckICMPHeader::reason_texts[NREASONS] = {
  "not ICMP", "bad packet length", "bad ICMP checksum"
};

CheckICMPHeader::CheckICMPHeader()
  : _reason_drops(0)
{
  _drops = 0;
}

CheckICMPHeader::~CheckICMPHeader()
{
  delete[] _reason_drops;
}

int
CheckICMPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool verbose = false, details = false;

    if (Args(conf, this, errh)
	.read("VERBOSE", verbose)
	.read("DETAILS", details)
	.complete() < 0)
	return -1;

  _verbose = verbose;
  if (details)
    _reason_drops = new atomic_uint32_t[NREASONS];

  return 0;
}


Packet *
CheckICMPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("%p{element}: ICMP header check failed: %s", this, reason_texts[reason]);
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
CheckICMPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  unsigned csum, icmp_len;
  const click_icmp *icmph = p->icmp_header();

  if (!p->has_network_header() || iph->ip_p != IP_PROTO_ICMP)
    return drop(NOT_ICMP, p);

  icmp_len = p->length() - p->transport_header_offset();
  if (icmp_len < sizeof(click_icmp))
    return drop(BAD_LENGTH, p);

  switch (icmph->icmp_type) {

   case ICMP_UNREACH:
   case ICMP_TIMXCEED:
   case ICMP_PARAMPROB:
   case ICMP_SOURCEQUENCH:
   case ICMP_REDIRECT:
    // check for IP header + first 64 bits of datagram = at least 28 bytes
    if (icmp_len < sizeof(click_icmp) + 28)
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_TSTAMP:
   case ICMP_TSTAMPREPLY:
    // exactly 12 more bytes
    if (icmp_len != sizeof(click_icmp_tstamp))
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_IREQ:
   case ICMP_IREQREPLY:
    // exactly 0 more bytes
    if (icmp_len != sizeof(click_icmp))
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_ROUTERADVERT:
    /* nada */
   case ICMP_MASKREQ:
   case ICMP_MASKREQREPLY:
    /* nada */

   case ICMP_ECHO:
   case ICMP_ECHOREPLY:
   default:
    // no additional length checks
    break;

  }

  csum = click_in_cksum((unsigned char *)icmph, icmp_len) & 0xFFFF;
  if (csum != 0)
    return drop(BAD_CHECKSUM, p);

  return p;
}

String
CheckICMPHeader::read_handler(Element *e, void *thunk)
{
  CheckICMPHeader *c = reinterpret_cast<CheckICMPHeader *>(e);
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
CheckICMPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, 0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckICMPHeader)

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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#ifdef CLICK_LINUXMODULE
# include <net/checksum.h>
#endif

const char *CheckICMPHeader::reason_texts[NREASONS] = {
  "not ICMP", "bad packet length", "bad ICMP checksum"
};

CheckICMPHeader::CheckICMPHeader()
  : Element(1, 1), _reason_drops(0)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

CheckICMPHeader::~CheckICMPHeader()
{
  MOD_DEC_USE_COUNT;
  delete[] _reason_drops;
}

void
CheckICMPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckICMPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
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
CheckICMPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("ICMP header check failed: %s", reason_texts[reason]);
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
  const icmp_generic *icmph = reinterpret_cast<const icmp_generic *>(p->transport_header());
  
  if (!iph || iph->ip_p != IP_PROTO_ICMP)
    return drop(NOT_ICMP, p);
  
  icmp_len = p->length() - p->transport_header_offset();
  if ((int)icmp_len < (int)sizeof(icmp_generic))
    return drop(BAD_LENGTH, p);

  switch (icmph->icmp_type) {

   case ICMP_DST_UNREACHABLE:
   case ICMP_TYPE_TIME_EXCEEDED:
   case ICMP_PARAMETER_PROBLEM:
   case ICMP_SOURCE_QUENCH:
   case ICMP_REDIRECT:
    // check for IP header + first 64 bits of datagram = at least 28 bytes
    if (icmp_len < sizeof(icmp_generic) + 28)
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_TIME_STAMP:
   case ICMP_TIME_STAMP_REPLY:
    // exactly 12 more bytes
    if (icmp_len != sizeof(icmp_generic) + 12)
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_INFO_REQUEST:
   case ICMP_INFO_REQUEST_REPLY:
    // exactly 0 more bytes
    if (icmp_len != sizeof(icmp_generic))
      return drop(BAD_LENGTH, p);
    break;

   case ICMP_ECHO:
   case ICMP_ECHO_REPLY:
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
CheckICMPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

EXPORT_ELEMENT(CheckICMPHeader)

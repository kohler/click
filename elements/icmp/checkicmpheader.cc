/*
 * checkicmpheader.{cc,hh} -- element checks ICMP header for correctness
 * (checksums, lengths)
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/config.h>
#include <click/package.hh>
#include "checkicmpheader.hh"
#include <click/click_ip.h>
#include <click/click_icmp.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/bitvector.hh>
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckICMPHeader::CheckICMPHeader()
  : Element(1, 1), _drops(0)
{
  MOD_INC_USE_COUNT;
}

CheckICMPHeader::~CheckICMPHeader()
{
  MOD_DEC_USE_COUNT;
}

void
CheckICMPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

Packet *
CheckICMPHeader::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  unsigned csum, icmp_len;
  const icmp_generic *icmph = reinterpret_cast<const icmp_generic *>(p->transport_header());
  
  if (!iph || iph->ip_p != IP_PROTO_ICMP)
    goto bad;

  icmp_len = p->length() - p->transport_header_offset();
  if ((int)icmp_len < (int)sizeof(icmp_generic))
    goto bad;

  switch (icmph->icmp_type) {

   case ICMP_DST_UNREACHABLE:
   case ICMP_TYPE_TIME_EXCEEDED:
   case ICMP_PARAMETER_PROBLEM:
   case ICMP_SOURCE_QUENCH:
   case ICMP_REDIRECT:
    /* check for IP header + first 64 bits of datagram = at least 28 bytes */
    if (icmp_len < sizeof(icmp_generic) + 28)
      goto bad;
    break;

   case ICMP_TIME_STAMP:
   case ICMP_TIME_STAMP_REPLY:
    // exactly 12 more bytes
    if (icmp_len != sizeof(icmp_generic) + 12)
      goto bad;
    break;

   case ICMP_INFO_REQUEST:
   case ICMP_INFO_REQUEST_REPLY:
    // exactly 0 more bytes
    if (icmp_len != sizeof(icmp_generic))
      goto bad;
    break;

   case ICMP_ECHO:
   case ICMP_ECHO_REPLY:
   default:
    // no additional length checks
    break;

  }

  csum = in_cksum((unsigned char *)icmph, icmp_len) & 0xFFFF;
  if (csum != 0)
    goto bad;

  return p;
  
 bad:
  if (_drops == 0)
    click_chatter("ICMP header check failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  
  return 0;
}

static String
CheckICMPHeader_read_drops(Element *thunk, void *)
{
  CheckICMPHeader *e = (CheckICMPHeader *)thunk;
  return String(e->drops()) + "\n";
}

void
CheckICMPHeader::add_handlers()
{
  add_read_handler("drops", CheckICMPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckICMPHeader)

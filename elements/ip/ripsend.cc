/*
 * ripsend.{cc,hh} -- element advertises routes using RIP protocol
 * Robert Morris
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
#include "ripsend.hh"
#include <click/args.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
CLICK_DECLS

RIPSend::RIPSend()
  : _timer(this)
{
}

RIPSend::~RIPSend()
{
}

int
RIPSend::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("SRC", _src)
	.read_mp("DST", _dst)
	.read_mp("PREFIX", IPPrefixArg(), _what, _mask)
	.read_mp("METRIC", _metric).complete();
}

int
RIPSend::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(3 * 1000);
  return 0;
}

void
RIPSend::run_timer(Timer *)
{
  WritablePacket *p = Packet::make(sizeof(click_ip) + sizeof(click_udp) + 24);
  memset(p->data(), '\0', p->length());

  /* for now just pseudo-header fields for UDP checksum */
  click_ip *ipp = reinterpret_cast<click_ip *>(p->data());
  ipp->ip_len = htons(p->length() - sizeof(*ipp));
  ipp->ip_p = IPPROTO_UDP;
  ipp->ip_src = _src.in_addr();
  ipp->ip_dst = _dst.in_addr();

  /* RIP payload */
  click_udp *udpp = reinterpret_cast<click_udp *>(ipp + 1);
  unsigned int *r = (unsigned int *) (udpp + 1);
  r[0] = htonl((2 << 24) | (2 << 16) | 0);
  r[1] = htonl((2 << 16) | 0);
  r[2] = _what.addr();
  r[3] = _mask.addr();
  r[4] = _src.addr();
  r[5] = htonl(_metric);

  /* UDP header */
  udpp->uh_sport = htons(520);
  udpp->uh_dport = htons(520);
  udpp->uh_ulen = htons(p->length() - sizeof(*ipp));
  udpp->uh_sum = click_in_cksum(p->data(), p->length());

  /* the remaining IP header fields */
  ipp->ip_len = htons(p->length());
  ipp->ip_hl = sizeof(click_ip) >> 2;
  ipp->ip_v = 4;
  ipp->ip_ttl = 200;
  ipp->ip_sum = click_in_cksum((unsigned char *) ipp, sizeof(*ipp));

  p->set_ip_header(ipp, sizeof(click_ip));

  output(0).push(p);

  _timer.schedule_after_msec(30 * 1000);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RIPSend)

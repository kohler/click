/*
 * icmpsendpings.{cc,hh} -- Send ICMP ping packets.
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

#include <click/config.h>
#include <click/package.hh>
#include "icmpsendpings.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_ip.h>
#include <click/click_icmp.h>
#include <click/packet_anno.hh>

ICMPSendPings::ICMPSendPings()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
  add_output();
  _id = 1;
}

ICMPSendPings::~ICMPSendPings()
{
  MOD_DEC_USE_COUNT;
}

ICMPSendPings *
ICMPSendPings::clone() const
{
  return new ICMPSendPings;
}

int
ICMPSendPings::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
                     cpIPAddress, "source IP address", &_src,
                     cpIPAddress, "destination IP address", &_dst,
		     0);
}

int
ICMPSendPings::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(1000);
  return 0;
}

void
ICMPSendPings::uninitialize()
{
  _timer.unschedule();
}

void
ICMPSendPings::run_scheduled()
{
  WritablePacket *q = Packet::make(sizeof(click_ip) +
                                   sizeof(struct icmp_generic));
  memset(q->data(), '\0', q->length());

  click_ip *nip = reinterpret_cast<click_ip *>(q->data());
  nip->ip_v = 4;
  nip->ip_hl = sizeof(click_ip) >> 2;
  nip->ip_len = htons(q->length());
  nip->ip_id = htons(_id++);
  nip->ip_p = IP_PROTO_ICMP; /* icmp */
  nip->ip_ttl = 200;
  nip->ip_src = _src;
  nip->ip_dst = _dst;
  nip->ip_sum = in_cksum((unsigned char *)nip, sizeof(click_ip));

  icmp_generic *icp = (struct icmp_generic *) (nip + 1);
  icp->icmp_type = ICMP_ECHO;
  icp->icmp_code = 0;

  icp->icmp_cksum = in_cksum((unsigned char *)icp, sizeof(icmp_generic));

  q->set_dst_ip_anno(IPAddress(_dst));
  q->set_ip_header(nip, sizeof(click_ip));

  output(0).push(q);

  _timer.schedule_after_ms(1000);
}

EXPORT_ELEMENT(ICMPSendPings)

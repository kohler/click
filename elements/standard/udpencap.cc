/*
 * udpencap.{cc,hh} -- element encapsulates packet in UDP header
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "click_ip.h"
#include "udpencap.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

UDPEncap::UDPEncap()
  : Element(1, 1), _cksum(1), _sport(1234), _dport(1234)
{
}

UDPEncap::~UDPEncap()
{
}

UDPEncap *
UDPEncap::clone() const
{
  return new UDPEncap;
}

int
UDPEncap::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "source port", &_sport,
		  cpUnsigned, "destination port", &_dport,
		  cpBool, "turn on/off checksum", &_cksum,
		  0) < 0)
    return -1;
  return 0;
}

int
UDPEncap::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
UDPEncap::simple_action(Packet *p)
{
  p = p->push(sizeof(click_udp));
  click_udp *udp = (click_udp *) p->data();
  memset(udp, '\0', sizeof(click_udp));
  
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  udp->uh_ulen = htons(p->length());
  udp->uh_sum = (_cksum ? in_cksum(p->data(), p->length()) : 0);
  return p;
}

EXPORT_ELEMENT(UDPEncap)

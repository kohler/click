// -*- c-basic-offset: 2; related-file-name: "../include/click/ip6flowid.hh" -*-
/*
 * ip6flowid.{cc,hh} -- a TCP-UDP/IP connection class.
 * Eddie Kohler, Peilei Fan
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
#include <click/glue.hh>
#include <click/ip6flowid.hh>
#include <clicknet/ip6.h>
#include <clicknet/udp.h>
#include <click/packet.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

IP6FlowID::IP6FlowID(Packet *p)
{
  const click_ip6 *iph = p->ip6_header();
  _saddr = IP6Address(iph->ip6_src);
  _daddr = IP6Address(iph->ip6_dst);

  const click_udp *udph = p->udp_header();
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

String
IP6FlowID::unparse() const
{
  StringAccum sa;
  sa << '(' << _saddr.unparse() << ", " << ntohs(_sport) << ", "
     << _daddr.unparse() << ", " << ntohs(_dport) << ')';
  return sa.take_string();
}

#if CLICK_USERLEVEL
int IP6FlowID_linker_trick;
#endif

CLICK_ENDDECLS

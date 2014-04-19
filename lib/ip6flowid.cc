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

IP6FlowID::IP6FlowID(const Packet *p, bool reverse)
{
  assert(p->has_network_header() && p->has_transport_header());
  const click_ip6 *ip6h = p->ip6_header();
  const click_ip *iph = p->ip_header();
  const click_udp *udph = p->udp_header();

  if (ip6h->ip6_v == 6) {
    if (likely(!reverse)) {
      assign(IP6Address(ip6h->ip6_src), udph->uh_sport, IP6Address(ip6h->ip6_dst), udph->uh_dport);
    } else {
      assign(IP6Address(ip6h->ip6_dst), udph->uh_dport, IP6Address(ip6h->ip6_src), udph->uh_sport);
    }
  } else {
    assert(IP_FIRSTFRAG(iph));
    if (likely(!reverse))
      assign(iph->ip_src, udph->uh_sport,
             iph->ip_dst, udph->uh_dport);
    else
      assign(iph->ip_dst, udph->uh_dport,
             iph->ip_src, udph->uh_sport);
  }
}

IP6FlowID::IP6FlowID(const click_ip6 *ip6h, bool reverse)
{
  assert(ip6h);
  const click_udp *udph = reinterpret_cast<const click_udp *>(reinterpret_cast<const unsigned char *>(ip6h) + sizeof(click_ip6));

  if (likely(!reverse)) {
    assign(IP6Address(ip6h->ip6_src), udph->uh_sport, IP6Address(ip6h->ip6_dst), udph->uh_dport);
  } else {
    assign(IP6Address(ip6h->ip6_dst), udph->uh_dport, IP6Address(ip6h->ip6_src), udph->uh_sport);
  }
}

IP6FlowID::IP6FlowID(const click_ip *iph, bool reverse)
{
  assert(iph && IP_FIRSTFRAG(iph));
  const click_udp *udph = reinterpret_cast<const click_udp *>(reinterpret_cast<const unsigned char *>(iph) + (iph->ip_hl << 2));

  if (likely(!reverse))
    assign(iph->ip_src, udph->uh_sport,
           iph->ip_dst, udph->uh_dport);
  else
    assign(iph->ip_dst, udph->uh_dport,
           iph->ip_src, udph->uh_sport);

}

IPFlowID
IP6FlowID::flow_id4() const
{
  if (is_ip4_mapped())
    return IPFlowID(saddr4(),_sport,daddr4(),_dport);
  else
    return IPFlowID();
}

IP6FlowID
IP6FlowID::flow_id6() const
{
    return *this;
}

String
IP6FlowID::unparse() const
{
  StringAccum sa;
  sa << '(' << _saddr.unparse() << ", " << ntohs(_sport) << ", "
     << _daddr.unparse() << ", " << ntohs(_dport) << ')';
  return sa.take_string();
}

StringAccum &
operator<<(StringAccum &sa, const IP6FlowID &flow_id)
{
  sa << flow_id.unparse();
  return sa;
}

#if CLICK_USERLEVEL
int IP6FlowID_linker_trick;
#endif

CLICK_ENDDECLS

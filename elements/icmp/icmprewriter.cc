/* -*- c-basic-offset: 2 -*- */
/*
 * icmprewriter.{cc,hh} -- rewrites ICMP non-echoes and non-replies
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
#include "icmprewriter.hh"
#include <click/click_icmp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>

ICMPRewriter::ICMPRewriter()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

ICMPRewriter::~ICMPRewriter()
{
  MOD_DEC_USE_COUNT;
}

void
ICMPRewriter::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
ICMPRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String arg;
  if (cp_va_parse(conf, this, errh,
		  cpArgument, "rewriters", &arg,
		  0) < 0)
    return -1;

  Vector<String> words;
  cp_spacevec(arg, words);

  for (int i = 0; i < words.size(); i++) {
    if (Element *e = cp_element(words[i], this, errh)) {
      if (IPRw *rw = static_cast<IPRw *>(e->cast("IPRw")))
	_maps.push_back(rw);
      else if (ICMPPingRewriter *rw = static_cast<ICMPPingRewriter *>(e->cast("ICMPPingRewriter")))
	_ping_maps.push_back(rw);
      else
	errh->error("element `%s' is not an IP rewriter", words[i].cc());
    }
  }

  if (_maps.size() == 0 && _ping_maps.size() == 0)
    return errh->error("no IP rewriters supplied");
  return 0;
}

void
ICMPRewriter::rewrite_packet(WritablePacket *p, click_ip *embedded_iph,
			     click_udp *embedded_udph, const IPFlowID &flow,
			     IPRw::Mapping *mapping)
{
  click_ip *iph = p->ip_header();
  icmp_generic *icmph = reinterpret_cast<icmp_generic *>(p->transport_header());

  // XXX incremental checksums?
  
  IPFlowID new_flow = mapping->flow_id().rev();
  
  // change IP header destination if appropriate
  if (IPAddress(iph->ip_dst) == flow.saddr()) {
    unsigned hlen = iph->ip_hl << 2;
    iph->ip_dst = new_flow.saddr();
    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum((unsigned char *)iph, hlen);
  }
  
  // don't bother patching embedded IP or UDP checksums
  embedded_iph->ip_src = new_flow.saddr();
  embedded_iph->ip_dst = new_flow.daddr();
  embedded_udph->uh_sport = new_flow.sport();
  embedded_udph->uh_dport = new_flow.dport();

  // but must patch ICMP checksum
  icmph->icmp_cksum = 0;
  icmph->icmp_cksum = click_in_cksum((unsigned char *)icmph, p->length() - p->transport_header_offset());
}

void
ICMPRewriter::rewrite_ping_packet(WritablePacket *p, click_ip *embedded_iph,
				  icmp_sequenced *embedded_icmph, const IPFlowID &flow,
				  ICMPPingRewriter::Mapping *mapping)
{
  click_ip *iph = p->ip_header();
  icmp_generic *icmph = reinterpret_cast<icmp_generic *>(p->transport_header());

  // XXX incremental checksums?
  
  IPFlowID new_flow = mapping->flow_id().rev();
  
  // change IP header destination if appropriate
  if (IPAddress(iph->ip_dst) == flow.saddr()) {
    unsigned hlen = iph->ip_hl << 2;
    iph->ip_dst = new_flow.saddr();
    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum((unsigned char *)iph, hlen);
  }
  
  // don't bother patching embedded ICMP checksum
  embedded_iph->ip_src = new_flow.saddr();
  embedded_iph->ip_dst = new_flow.daddr();
  embedded_icmph->identifier = new_flow.sport();

  // but must patch ICMP checksum
  icmph->icmp_cksum = 0;
  icmph->icmp_cksum = click_in_cksum((unsigned char *)icmph, p->length() - p->transport_header_offset());
}

void
ICMPRewriter::push(int, Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *iph = p->ip_header();
  
  if (iph->ip_p != IP_PROTO_ICMP) {
    p_in->kill();
    return;
  }
  
  icmp_generic *icmph = reinterpret_cast<icmp_generic *>(p->transport_header());
  switch (icmph->icmp_type) {

   case ICMP_DST_UNREACHABLE:
   case ICMP_TYPE_TIME_EXCEEDED:
   case ICMP_PARAMETER_PROBLEM:
   case ICMP_SOURCE_QUENCH:
   case ICMP_REDIRECT: {
     // check length of embedded IP header
     click_ip *embedded_iph = reinterpret_cast<click_ip *>(icmph + 1);
     unsigned hlen = embedded_iph->ip_hl << 2;
     if (p->length() - p->transport_header_offset() < sizeof(icmp_generic) + hlen + 8
	 || hlen < sizeof(click_ip))
       goto bad;

     // check protocol
     int embedded_p = embedded_iph->ip_p;

     if (embedded_p == IP_PROTO_UDP || embedded_p == IP_PROTO_TCP) {
       // TCP or UDP
       // create flow ID
       click_udp *embedded_udph = reinterpret_cast<click_udp *>(reinterpret_cast<unsigned char *>(embedded_iph) + hlen);
       IPFlowID flow(embedded_iph->ip_src, embedded_udph->uh_sport, embedded_iph->ip_dst, embedded_udph->uh_dport);
     
       IPRw::Mapping *mapping = 0;
       for (int i = 0; i < _maps.size() && !mapping; i++)
	 mapping = _maps[i]->get_mapping(embedded_p, flow.rev());
       if (!mapping)
	 goto unmapped;
     
       rewrite_packet(p, embedded_iph, embedded_udph, flow, mapping);
       
     } else if (embedded_p == IP_PROTO_ICMP) {
       // ICMP
       icmp_sequenced *embedded_icmph = reinterpret_cast<icmp_sequenced *>(reinterpret_cast<unsigned char *>(embedded_iph) + hlen);
       
       int embedded_type = embedded_icmph->icmp_type;
       if (embedded_type != ICMP_ECHO && embedded_type != ICMP_ECHO_REPLY)
	 goto unmapped;
       bool ask_for_request = (embedded_type != ICMP_ECHO);
       
       IPFlowID flow(embedded_iph->ip_src, embedded_icmph->identifier, embedded_iph->ip_dst, embedded_icmph->identifier);

       ICMPPingRewriter::Mapping *mapping = 0;
       for (int i = 0; i < _ping_maps.size() && !mapping; i++)
	 mapping = _ping_maps[i]->get_mapping(ask_for_request, flow.rev());
       if (!mapping)
	 goto unmapped;

       rewrite_ping_packet(p, embedded_iph, embedded_icmph, flow, mapping);
       
     } else
       goto unmapped;
       
     output(0).push(p);
     break;
   }

   bad:
    p->kill();
    break;

   unmapped:
   default:
    if (noutputs() == 1)
      p->kill();
    else
      output(1).push(p);
    break;

  }
}

ELEMENT_REQUIRES(IPRw ICMPPingRewriter)
EXPORT_ELEMENT(ICMPRewriter)

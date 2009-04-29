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
#include <clicknet/icmp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

ICMPRewriter::ICMPRewriter()
{
}

ICMPRewriter::~ICMPRewriter()
{
}

int
ICMPRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String arg;
  _dst_anno = true;
  if (cp_va_kparse(conf, this, errh,
		   "MAPS", cpkP+cpkM, cpArgument, &arg,
		   "DST_ANNO", 0, cpBool, &_dst_anno,
		   cpEnd) < 0)
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
	errh->error("element `%s' is not an IP rewriter", words[i].c_str());
    }
  }

  if (_maps.size() == 0 && _ping_maps.size() == 0)
    return errh->error("no IP rewriters supplied");
  return 0;
}

static void
update_in_cksum(uint16_t *csum, const uint16_t *old_hw, const uint16_t *new_hw,
		int nhw)
{
    for (; nhw > 0; --nhw, ++old_hw, ++new_hw)
	click_update_in_cksum(csum, *old_hw, *new_hw);
}

void
ICMPRewriter::rewrite_packet(WritablePacket *p, click_ip *embedded_iph,
			     click_udp *embedded_udph, const IPFlowID &flow,
			     IPRw::Mapping *mapping)
{
    click_ip *iph = p->ip_header();
    click_icmp *icmph = p->icmp_header();

    uint16_t old_hw[8], new_hw[8];
    uint16_t *embedded_csum = 0;
    if (embedded_iph->ip_p == IP_PROTO_UDP
	&& reinterpret_cast<uint8_t *>(embedded_udph + 1) <= p->end_data()
	&& embedded_udph->uh_sum)
	embedded_csum = &embedded_udph->uh_sum;
    else if (embedded_iph->ip_p == IP_PROTO_TCP
	     && reinterpret_cast<uint8_t *>(embedded_udph) + 18 <= p->end_data())
	embedded_csum = reinterpret_cast<uint16_t *>(embedded_udph) + 8;

    IPFlowID new_flow = mapping->flow_id().reverse();

    // change IP header destination if appropriate
    if (IPAddress(iph->ip_dst) == flow.saddr()) {
	memcpy(old_hw, &iph->ip_dst, 4);
	iph->ip_dst = new_flow.saddr();
	update_in_cksum(&iph->ip_sum, old_hw, reinterpret_cast<const uint16_t *>(&iph->ip_dst), 2);
	if (_dst_anno)
	    p->set_dst_ip_anno(new_flow.saddr());
    }

    // change embedded IP and TCP/UDP headers
    memcpy(old_hw, &embedded_iph->ip_src, 8);
    memcpy(old_hw + 4, &embedded_udph->uh_sport, 4);
    embedded_iph->ip_src = new_flow.saddr();
    embedded_iph->ip_dst = new_flow.daddr();
    embedded_udph->uh_sport = new_flow.sport();
    embedded_udph->uh_dport = new_flow.dport();
    memcpy(new_hw, &embedded_iph->ip_src, 8);
    memcpy(new_hw + 4, &embedded_udph->uh_sport, 4);

    // patch embedded TCP/UDP checksum, if it exists
    if (embedded_csum) {
	old_hw[7] = *embedded_csum;
	update_in_cksum(embedded_csum, old_hw, new_hw, 6);
	new_hw[7] = *embedded_csum;
    }
    // patch embedded IP checksum
    old_hw[6] = embedded_iph->ip_sum;
    update_in_cksum(&embedded_iph->ip_sum, old_hw, new_hw, 4);
    new_hw[6] = embedded_iph->ip_sum;

    // patch outer ICMP checksum
    update_in_cksum(&icmph->icmp_cksum, old_hw, new_hw, 7 + (embedded_csum ? 1 : 0));
}

void
ICMPRewriter::rewrite_ping_packet(WritablePacket *p, click_ip *embedded_iph,
				  click_icmp_echo *embedded_icmph, const IPFlowID &flow,
				  ICMPPingRewriter::Mapping *mapping)
{
    click_ip *iph = p->ip_header();
    click_icmp *icmph = p->icmp_header();

    uint16_t old_hw[7], new_hw[7];

    IPFlowID new_flow = mapping->flow_id().reverse();

    // change IP header destination if appropriate
    if (IPAddress(iph->ip_dst) == flow.saddr()) {
	memcpy(old_hw, &iph->ip_dst, 4);
	iph->ip_dst = new_flow.saddr();
	update_in_cksum(&iph->ip_sum, old_hw, reinterpret_cast<const uint16_t *>(&iph->ip_dst), 2);
	if (_dst_anno)
	    p->set_dst_ip_anno(new_flow.saddr());
    }

    // change embedded IP and ICMP headers
    memcpy(old_hw, &embedded_iph->ip_src, 8);
    memcpy(old_hw + 4, &embedded_icmph->icmp_cksum, 4);
    embedded_iph->ip_src = new_flow.saddr();
    embedded_iph->ip_dst = new_flow.daddr();
    embedded_icmph->icmp_identifier = new_flow.sport();
    click_update_in_cksum(&embedded_icmph->icmp_cksum, old_hw[5], new_flow.sport());
    memcpy(new_hw, &embedded_iph->ip_src, 8);
    memcpy(new_hw + 4, &embedded_icmph->icmp_cksum, 4);

    // patch embedded IP checksum
    old_hw[6] = embedded_iph->ip_sum;
    update_in_cksum(&embedded_iph->ip_sum, old_hw, new_hw, 4);
    new_hw[6] = embedded_iph->ip_sum;

    // patch outer ICMP checksum
    update_in_cksum(&icmph->icmp_cksum, old_hw, new_hw, 7);
}

Packet *
ICMPRewriter::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  if (!p)
    return 0;
  else if (p->ip_header()->ip_p != IP_PROTO_ICMP) {
    p->kill();
    return 0;
  }

  click_icmp *icmph = p->icmp_header();
  switch (icmph->icmp_type) {

   case ICMP_UNREACH:
   case ICMP_TIMXCEED:
   case ICMP_PARAMPROB:
   case ICMP_SOURCEQUENCH:
   case ICMP_REDIRECT: {
     // check length of embedded IP header
     click_ip *embedded_iph = reinterpret_cast<click_ip *>(icmph + 1);
     unsigned hlen = embedded_iph->ip_hl << 2;
     if (p->transport_length() < (int)(sizeof(click_icmp) + hlen + 8)
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
	 mapping = _maps[i]->get_mapping(embedded_p, flow.reverse());
       if (!mapping)
	 goto unmapped;

       rewrite_packet(p, embedded_iph, embedded_udph, flow, mapping);

     } else if (embedded_p == IP_PROTO_ICMP) {
       // ICMP
       click_icmp_sequenced *embedded_icmph = reinterpret_cast<click_icmp_sequenced *>(reinterpret_cast<unsigned char *>(embedded_iph) + hlen);

       int embedded_type = embedded_icmph->icmp_type;
       if (embedded_type != ICMP_ECHO && embedded_type != ICMP_ECHOREPLY)
	 goto unmapped;
       bool ask_for_request = (embedded_type != ICMP_ECHO);

       IPFlowID flow(embedded_iph->ip_src, embedded_icmph->icmp_identifier, embedded_iph->ip_dst, embedded_icmph->icmp_identifier);

       ICMPPingRewriter::Mapping *mapping = 0;
       for (int i = 0; i < _ping_maps.size() && !mapping; i++)
	 mapping = _ping_maps[i]->get_mapping(ask_for_request, flow.reverse());
       if (!mapping)
	 goto unmapped;

       rewrite_ping_packet(p, embedded_iph, embedded_icmph, flow, mapping);

     } else
       goto unmapped;

     return p;
   }

   bad:
    p->kill();
    return 0;

   unmapped:
   default:
    checked_output_push(1, p);
    return 0;

  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRw ICMPPingRewriter)
EXPORT_ELEMENT(ICMPRewriter)

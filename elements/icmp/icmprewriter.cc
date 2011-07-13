/*
 * icmprewriter.{cc,hh} -- rewrites ICMP non-echoes and non-replies
 * Eddie Kohler, Cliff Frey
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2009-2010 Meraki, Inc.
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
#include "elements/ip/iprwmapping.hh"
#include <clicknet/icmp.h>
#include <clicknet/tcp.h>
#include <click/args.hh>
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
    bool dst_anno = true, has_reply_anno = false;
    int reply_anno;
    if (Args(conf, this, errh)
	.read_mp("MAPS", AnyArg(), arg)
	.read("DST_ANNO", dst_anno)
	.read("REPLY_ANNO", AnnoArg(1), reply_anno).read_status(has_reply_anno)
	.complete() < 0)
	return -1;

    _annos = (dst_anno ? 1 : 0) + (has_reply_anno ? 2 + (reply_anno << 2) : 0);

    Vector<String> words;
    cp_spacevec(arg, words);

    for (int i = 0; i < words.size(); i++) {
	String eltname = words[i];
	int port_offset = -1;
	int colon = eltname.find_left(':');
	if (colon >= 0) {
	    if (!IntArg().parse(eltname.substring(colon+1), port_offset)) {
	    parse_problem:
		errh->error("%s not an IP rewriter", eltname.c_str());
		continue;
	    }
	    eltname = words[i].substring(0, colon);
	}
	Element *e;
	if (!(e = cp_element(eltname, this)))
	    goto parse_problem;
	IPRewriterBase *rw;
	if (port_offset >= 0 && port_offset + e->noutputs() > noutputs()) {
	    errh->error("%s port offset requires more than %d %s", words[i].c_str(), noutputs(), noutputs() == 1 ? "output" : "outputs");
	    continue;
	}
	if ((rw = static_cast<IPRewriterBase *>(e->cast("IPRewriterBase"))))
	    _maps.push_back(MapEntry(rw, port_offset));
	else
	    goto parse_problem;
    }

    if (_maps.size() == 0)
	return errh->error("no IP rewriters supplied");
    return errh->nerrors() ? -1 : 0;
}

static void
update_in_cksum(uint16_t *csum, const uint16_t *old_hw, const uint16_t *new_hw,
		int nhw)
{
    for (; nhw > 0; --nhw, ++old_hw, ++new_hw)
	click_update_in_cksum(csum, *old_hw, *new_hw);
}

int
ICMPRewriter::handle(WritablePacket *p)
{
    // check ICMP type
    click_icmp *icmph = p->icmp_header();
    if (icmph->icmp_type != ICMP_UNREACH
	&& icmph->icmp_type != ICMP_TIMXCEED
	&& icmph->icmp_type != ICMP_PARAMPROB
	&& icmph->icmp_type != ICMP_SOURCEQUENCH
	&& icmph->icmp_type != ICMP_REDIRECT)
	return unmapped_output;

    // check presence, version, & length of embedded IP header
    click_ip *enc_iph = reinterpret_cast<click_ip *>(icmph + 1);
    if (p->transport_length() < (int) (sizeof(click_icmp) + sizeof(click_ip)))
	return unmapped_output;
    uint8_t *enc_transp = reinterpret_cast<uint8_t *>(enc_iph) + (enc_iph->ip_hl << 2);
    if (enc_iph->ip_v != 4
	|| enc_transp < reinterpret_cast<uint8_t *>(enc_iph + 1))
	return unmapped_output;

    // look up flow ID, may require looking into encapsulated headers
    int enc_p = enc_iph->ip_p;
    IPFlowID search_flowid(enc_iph->ip_dst, 0, enc_iph->ip_src, 0);
    bool use_enc_transp = false;
    if (enc_p == IP_PROTO_TCP
	|| enc_p == IP_PROTO_UDP
	|| enc_p == IP_PROTO_DCCP) {
	if (enc_transp + 8 <= p->end_data()
	    && IP_FIRSTFRAG(enc_iph)) {
	    click_udp *enc_udph = reinterpret_cast<click_udp *>(enc_transp);
	    search_flowid.set_sport(enc_udph->uh_dport);
	    search_flowid.set_dport(enc_udph->uh_sport);
	    use_enc_transp = true;
	}
    } else if (enc_p == IP_PROTO_ICMP) {
	click_icmp_echo *enc_icmph = reinterpret_cast<click_icmp_echo *>(enc_transp);
	if (enc_transp + 6 <= p->end_data()
	    && IP_FIRSTFRAG(enc_iph)
	    && enc_icmph->icmp_type == ICMP_ECHO) {
	    search_flowid.set_sport(enc_icmph->icmp_identifier);
	    use_enc_transp = true;
	}
    }

    // find mapping
    IPRewriterEntry *entry = 0;
    int mapid;
    for (mapid = 0; mapid < _maps.size(); ++mapid)
	if ((entry = _maps[mapid]._elt->get_entry(enc_p, search_flowid, IPRewriterBase::get_entry_reply)))
	    break;
    if (!entry)
	return unmapped_output;

    // rewrite packet
    IPFlowID new_flowid = entry->rewritten_flowid();

    // store changed halfwords for checksum updates
    // 0   - encapsulated IP checksum
    // 1-4 - encapsulated IP saddr, daddr
    // 5-6 - encapsulated TCP/UDP sport, dport
    // 7   - encapsulated TCP/UDP checksum
    // 5   - encapsulated ICMP ping identifier
    // 6   - encapsulated ICMP checksum
    int nhw;
    uint16_t old_hw[8], new_hw[8];

    // change IP header destination if appropriate
    if (IPAddress(p->ip_header()->ip_dst) == search_flowid.daddr()) {
	click_ip *iph = p->ip_header();
	memcpy(old_hw, &iph->ip_dst, 4);
	iph->ip_dst = new_flowid.daddr();
	update_in_cksum(&iph->ip_sum, old_hw, reinterpret_cast<const uint16_t *>(&iph->ip_dst), 2);
	if (_annos & 1)
	    p->set_dst_ip_anno(new_flowid.daddr());
    }
    if (entry->direction() && (_annos & 2))
	p->set_anno_u8(_annos >> 2, entry->flow()->reply_anno());

    // update encapsulated IP header
    memcpy(&old_hw[1], &enc_iph->ip_src, 8);
    old_hw[0] = enc_iph->ip_sum;
    enc_iph->ip_src = new_flowid.daddr();
    enc_iph->ip_dst = new_flowid.saddr(); // XXX source routing
    memcpy(&new_hw[1], &enc_iph->ip_src, 8);
    update_in_cksum(&enc_iph->ip_sum, old_hw + 1, new_hw + 1, 4);
    new_hw[0] = enc_iph->ip_sum;
    nhw = 5;

    // update encapsulated transport header, possibly including its checksum
    if (use_enc_transp) {
	if (enc_p == IP_PROTO_TCP
	    || enc_p == IP_PROTO_UDP
	    || enc_p == IP_PROTO_DCCP) {
	    click_udp *enc_udph = reinterpret_cast<click_udp *>(enc_transp);
	    memcpy(old_hw + nhw, &enc_udph->uh_sport, 4);
	    enc_udph->uh_sport = new_flowid.dport();
	    enc_udph->uh_dport = new_flowid.sport();
	    memcpy(new_hw + nhw, &enc_udph->uh_sport, 4);
	    nhw += 2;

	    uint16_t *enc_csum = 0;
	    if (enc_p == IP_PROTO_TCP && enc_transp + 18 <= p->end_data())
		enc_csum = &(reinterpret_cast<click_tcp *>(enc_transp)->th_sum);
	    else if (enc_p == IP_PROTO_UDP && enc_transp + 8 <= p->end_data()
		     && reinterpret_cast<click_udp *>(enc_transp)->uh_sum != 0)
		enc_csum = &(reinterpret_cast<click_udp *>(enc_transp)->uh_sum);
	    if (enc_csum) {
		old_hw[nhw] = *enc_csum;
		update_in_cksum(enc_csum, old_hw + 1, new_hw + 1, nhw - 1);
		new_hw[nhw] = *enc_csum;
		nhw++;
	    }

	} else if (enc_p == IP_PROTO_ICMP) {
	    click_icmp_echo *enc_icmph = reinterpret_cast<click_icmp_echo *>(enc_transp);
	    old_hw[nhw] = enc_icmph->icmp_identifier;
	    enc_icmph->icmp_identifier = new_hw[nhw] = new_flowid.dport();
	    nhw++;
	    old_hw[nhw] = enc_icmph->icmp_cksum;
	    click_update_in_cksum(&enc_icmph->icmp_cksum, old_hw[nhw-1], new_hw[nhw-1]);
	    click_update_zero_in_cksum(&enc_icmph->icmp_cksum, enc_transp, p->end_data() - enc_transp);
	    new_hw[nhw] = enc_icmph->icmp_cksum;
	    nhw++;
	}
    }

    // patch outer ICMP checksum
    update_in_cksum(&icmph->icmp_cksum, old_hw, new_hw, nhw);

    if (_maps[mapid]._port_offset >= 0)
	return _maps[mapid]._port_offset + entry->output();
    else
	return 0;
}

void
ICMPRewriter::push(int, Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    if (!p)
	return;
    else if (p->ip_header()->ip_p != IP_PROTO_ICMP) {
	p->kill();
	return;
    }

    int output = handle(p);
    checked_output_push(output, p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRewriterBase ICMPPingRewriter)
EXPORT_ELEMENT(ICMPRewriter)

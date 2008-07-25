// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_payload.{cc,hh} -- IP network layer IP summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2008 Regents of the University of California
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

#include "ipsumdumpinfo.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/md5.h>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_PAYLOAD_LEN, T_PAYLOAD, T_PAYLOAD_MD5 };

static void payload_info(Packet *p, const click_ip *iph,
			 int32_t &off, uint32_t &len)
{
    if (iph) {
	len = ntohs(iph->ip_len);
	off = p->transport_header_offset();
	if (IP_FIRSTFRAG(iph))
	    switch (iph->ip_p) {
	    case IP_PROTO_TCP:
		if (p->transport_length() >= 13
		    && ((uint32_t) off + (p->tcp_header()->th_off << 2) <= len
			|| len == 0))
		    off += (p->tcp_header()->th_off << 2);
		break;
	    case IP_PROTO_UDP:
	    case IP_PROTO_UDPLITE:
		if (off + sizeof(click_udp) <= len || len == 0)
		    off += sizeof(click_udp);
		break;
	    }
	len -= off - p->network_header_offset();
    } else {
	off = 0;
	len = p->length();
    }
}

static bool payload_extract(PacketDesc &d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_PAYLOAD_LEN: {
	int32_t off;
	payload_info(d.p, d.iph, off, d.v);
	if (!d.iph || d.force_extra_length)
	    d.v += EXTRA_LENGTH_ANNO(d.p);
	return true;
    }
      case T_PAYLOAD:
      case T_PAYLOAD_MD5:
	return true;
      default:
	return false;
    }
}

static void payload_inject(PacketOdesc &d, int thunk)
{
    if (d.make_ip(0))		// add default IPFlowID and protocol if nec.
	d.make_transp();	// don't care if we fail
    if (!d.p)
	return;

    int32_t off;
    uint32_t len;
    payload_info(d.p, d.is_ip ? d.p->ip_header() : 0, off, len);
    switch (thunk & ~B_TYPEMASK) {
    case T_PAYLOAD: {
	if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
	    return;
	uint32_t plen = d.vptr[1] - d.vptr[0];
	if (d.p->length() - off < plen
	    && !(d.p = d.p->put(plen - (d.p->length() - off))))
	    return;
	memcpy(d.p->data() + off, d.vptr[0], plen);
	break;
    }
    case T_PAYLOAD_LEN:
	if (d.p->length() - off < d.v)
	    SET_EXTRA_LENGTH_ANNO(d.p, d.v - (d.p->length() - off));
	break;
    }
}

static void payload_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_PAYLOAD:
    case T_PAYLOAD_MD5: {
	int32_t off;
	uint32_t len;
	payload_info(d.p, d.iph, off, len);
	if (off + len > (uint32_t) d.p->length())
	    len = d.p->length() - off;
	if ((thunk & ~B_TYPEMASK) == T_PAYLOAD) {
	    String s = String::stable_string((const char *)(d.p->data() + off), len);
	    *d.sa << cp_quote(s);
	} else {
	    md5_state_t pms;
	    md5_init(&pms);
	    md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);
	    if (char *buf = d.sa->extend(MD5_TEXT_DIGEST_SIZE))
		md5_finish_text(&pms, buf, 1);
	    md5_free(&pms);
	}
	break;
    }
    }
} 

static bool payload_ina(PacketOdesc& d, const String &str, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_PAYLOAD: {
	String s;
	if (cp_string(str, &s)) {
	    d.sa.clear();
	    d.sa << s;
	    d.vptr[0] = (const uint8_t *) d.sa.begin();
	    d.vptr[1] = (const uint8_t *) d.sa.end();
	    return true;
	}
	break;
    }
    }
    return false;
} 

static void payload_outb(const PacketDesc& d, bool, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_PAYLOAD_MD5: {
	int32_t off;
	uint32_t len;
	payload_info(d.p, d.iph, off, len);
	if (off + len > (uint32_t) d.p->length())
	    len = d.p->length() - off;
	md5_state_t pms;
	md5_init(&pms);
	md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);
	if (char *buf = d.sa->extend(MD5_DIGEST_SIZE))
	    md5_finish(&pms, (md5_byte_t *) buf);
	md5_free(&pms);
	break;
    }
    }
} 

void payload_register_unparsers()
{
    register_field("payload_len", T_PAYLOAD_LEN | B_4, ip_prepare, order_payload + 1,
		   payload_extract, payload_inject, num_outa, num_ina, outb, inb);
    register_field("payload", T_PAYLOAD | B_NOTALLOWED, ip_prepare, order_payload,
		   payload_extract, payload_inject, payload_outa, payload_ina, 0, 0);
    register_field("payload_md5", T_PAYLOAD_MD5 | B_16, ip_prepare, order_payload,
		   payload_extract, 0, payload_outa, 0, payload_outb, 0);

    register_synonym("payload_length", "payload_len");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Payload)
CLICK_ENDDECLS

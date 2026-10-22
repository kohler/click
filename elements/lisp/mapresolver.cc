/*
 * mapresolver.{cc,hh} -- Map-Resolver element: forwards Map-Requests, sends
 * Negative Map-Replies (LISPMapResolver)
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
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
#include "mapresolver.hh"
#include <click/ipaddress.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "lispcommon.hh"
#include <clicknet/ip.h>
#include <clicknet/udp.h>

CLICK_DECLS

/******************************************************************************************************************/

LISPMapResolver::LISPMapResolver()
{
    _rqst_received = _rply_sent = _response_found = _negative_sent
	= _ecm_received = 0;
}

/******************************************************************************************************************/

LISPMapResolver::~LISPMapResolver()
{
}

/******************************************************************************************************************/

int
LISPMapResolver::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    args.read_mp("MBase", ElementCastArg("LISPMappingDB"), _mbase);
    _hdr.read_args(args); // PROTO, SRC, DST and the IP keywords
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;

    return 0;
}

/********************************************************************************************************************/

int
LISPMapResolver::initialize(ErrorHandler *)
{
    return 0;
}

/********************************************************************************************************************/

/********************************************************************************************************************/

/******************************************************************************************************************/
// Map-Request from an ITR: forward it to the ETR registered for the EID
// (the ITR's address stays in the ITR-RLOC field, which the ETR answers),
// or answer with a Negative Map-Reply if no mapping covers the EID.
void
LISPMapResolver::push(int port, Packet *p)
{
    if (port) {
	p->kill();
	return;
    }
    ++_rqst_received;
    {
	// ECM (RFC 9301 section 5.8): strip the outer IP/UDP and ECM headers
	const click_ip *o = lisp_ip_header(p);
	const click_udp *ou = lisp_udp_header(o);
	const click_lisp_ecm *ecm
	    = reinterpret_cast<const click_lisp_ecm *>(ou + 1);
	if (reinterpret_cast<const uint8_t *>(ecm + 1) <= p->end_data()
	    && LISP_CTRL_TYPE(ntohl(ecm->type_flags)) == LISP_ECM) {
	    const click_ip *inner = reinterpret_cast<const click_ip *>(ecm + 1);
	    if (reinterpret_cast<const uint8_t *>(inner + 1) > p->end_data()) {
		p->kill();
		return;
	    }
	    p->pull(reinterpret_cast<const uint8_t *>(inner) - p->data());
	    p->set_ip_header(inner, inner->ip_hl << 2);
	    ++_ecm_received;
	}
    }
    const click_ip *in_iph = lisp_ip_header(p);
    const click_udp *in_udph = lisp_udp_header(in_iph);
    const click_lisp_map_request *mrh
	= reinterpret_cast<const click_lisp_map_request *>(in_udph + 1);
    if (reinterpret_cast<const uint8_t *>(mrh + 1) > p->end_data()) {
	p->kill(); // truncated Map-Request
	return;
    }
    _ip = in_iph->ip_src;
    bool prx_rply = false;
    IPAddress etr_rloc;
    IPAddress eid = mrh->rec_eid_prefix;
    if (!_mbase->mr_lookup(eid, mrh->rec_eid_mask_len, &prx_rply, &etr_rloc)) {
	if (Packet *q = negative_reply(in_iph, in_udph, mrh)) {
	    ++_negative_sent;
	    output(0).push(q);
	}
	p->kill();
	return;
    }
    ++_response_found;

    WritablePacket *q = p->uniqueify(); // before writing anything
    if (!q)
	return;
    click_ip *ip_hed = lisp_ip_header(q);
    click_udp *udph = lisp_udp_header(ip_hed);
    _itr_src = ip_hed->ip_src;
    _itr_dst = etr_rloc;
    _proto = ip_hed->ip_v;

    // new IP header, from the Map-Resolver to the ETR; same UDP ports
    if (reinterpret_cast<uint8_t *>(ip_hed) != q->data())
	q->pull(reinterpret_cast<uint8_t *>(ip_hed) - q->data());
    if (ip_hed->ip_hl != 5) {
	// keep the builder simple: drop IP options of the request
	int extra = (ip_hed->ip_hl << 2) - sizeof(click_ip);
	memmove(q->data() + extra, q->data(), sizeof(click_ip));
	q->pull(extra);
	udph = reinterpret_cast<click_udp *>(q->data() + sizeof(click_ip));
    }
    _hdr.fill(q, etr_rloc, udph->uh_sport, udph->uh_dport);
    LISPIPHeader::finish_udp(q); // addresses changed: new pseudo-header
    ++_rply_sent;
    output(0).push(q);
}

// Negative Map-Reply (RFC 9301 section 5.4): no locators, action
// Natively-Forward, TTL 15 minutes, sent to the ITR-RLOC of the request.
Packet *
LISPMapResolver::negative_reply(const click_ip *in_iph,
    const click_udp *in_udph, const click_lisp_map_request *mrh)
{
    const unsigned rec_len = 16; // record without locators
    WritablePacket *q = Packet::make(
	LISPIPHeader::headers + sizeof(click_lisp_map_reply) + rec_len);
    if (!q)
	return 0;
    memset(q->data(), 0, q->length());
    click_lisp_map_reply *rh = reinterpret_cast<click_lisp_map_reply *>(
	q->data() + LISPIPHeader::headers);
    Record *rec = reinterpret_cast<Record *>(rh + 1);

    IPAddress itr = in_iph->ip_src;
    if (mrh->itr_rloc_afi == htons(LISP_AFI_IP))
	itr = mrh->itr_rloc;
    _hdr.fill(
	q, itr, htons(LISP_CONTROL_PORT), in_udph->uh_sport); // to the ITR-RLOC

    rh->type_flags
	= htonl(((uint32_t)LISP_MAP_REPLY << LISP_CTRL_TYPE_SHIFT) | 1);
    rh->nonce1 = mrh->nonce1;
    rh->nonce2 = mrh->nonce2;
    int mask_len = mrh->rec_eid_mask_len <= 32 ? mrh->rec_eid_mask_len : 32;
    rec->rec_ttl = htonl(15);
    rec->loc_count = 0;
    rec->mask_len = mask_len;
    rec->act_a_reserved
	= htons(1 << LISP_REC_ACT_SHIFT); // ACT 1 = Natively-Forward
    rec->version = 0;
    rec->eid_afi = htons(LISP_AFI_IP);
    rec->eid_prefix = LISPPrefix(mrh->rec_eid_prefix, mask_len).addr.in_addr();

    LISPIPHeader::finish_udp(q);
    return q;
}

/******************************************************************************************************************/

/******************************************************************************************************************/

/******************************************************************************************************************/

void
LISPMapResolver::add_handlers()
{
    add_data_handlers("rqst received", Handler::OP_READ, &_rqst_received);
    add_data_handlers(
	"requests", Handler::OP_READ, &_rqst_received); // Click-style name
    add_data_handlers("response found", Handler::OP_READ, &_response_found);
    add_data_handlers(
	"found", Handler::OP_READ, &_response_found); // Click-style name
    add_data_handlers("rply sent", Handler::OP_READ, &_rply_sent);
    add_data_handlers(
	"forwarded", Handler::OP_READ, &_rply_sent); // Click-style name
    add_data_handlers("negative sent", Handler::OP_READ, &_negative_sent);
    add_data_handlers(
	"negative", Handler::OP_READ, &_negative_sent); // Click-style name
    add_data_handlers("ecm received", Handler::OP_READ, &_ecm_received);
    add_data_handlers(
	"ecm", Handler::OP_READ, &_ecm_received); // Click-style name
    add_data_handlers("ip ", Handler::OP_READ, &_ip);
    add_data_handlers("last_src", Handler::OP_READ, &_ip); // Click-style name
    add_data_handlers("ip address source", Handler::OP_READ, &_itr_src);
    add_data_handlers(
	"last_itr", Handler::OP_READ, &_itr_src); // Click-style name
    add_data_handlers("ip address destination", Handler::OP_READ, &_itr_dst);
    add_data_handlers(
	"last_etr", Handler::OP_READ, &_itr_dst); // Click-style name
    add_data_handlers("type protocol", Handler::OP_READ, &_proto);
    add_data_handlers(
	"last_ip_version", Handler::OP_READ, &_proto); // Click-style name
}

/******************************************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapResolver)
ELEMENT_REQUIRES(LISPMappingDB LISPCommon)
ELEMENT_MT_SAFE(LISPMapResolver)

/*
 * rqstsender1.{cc,hh} -- ITR element: map-cache lookups, Map-Request, Map-Reply
 * and Solicit-Map-Request handling (LISPMapRequester)
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
#include "rqstsender1.hh"
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

/********************************************************************************************************************/

CLICK_DECLS

/********************************************************************************************************************/

LISPMapRequester::LISPMapRequester()
  : _cache(0),
    _eid_mask_len(24),
    _aut_bit_set(0),
    _is_pitr(0),
    _reply_rec_includ(0),
    _smr_rply(0)
{
    _ecm = false;
    _lookup_miss = _lookup_hit = _all_resp = _valid_resp = _rqst_sent
	= _smr_count = _smr_ignored = _native_fwd = 0;
}

/********************************************************************************************************************/

LISPMapRequester::~LISPMapRequester()
{
}

/********************************************************************************************************************/

int
LISPMapRequester::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    args.read_mp("CACHE", ElementCastArg("LISPMapCache"), _cache);
    _hdr.read_args(args); // PROTO, SRC, DST and the IP keywords
    args.read(
	"MASK_LEN", _eid_mask_len); // EID mask-len in the request (default 24)
    args.read("AUTHORITATIVE", _aut_bit_set); // A bit
    args.read("MAP_DATA", _reply_rec_includ); // M bit
    args.read("PITR", _is_pitr);              // p bit
    args.read("SMR_INVOKED", _smr_rply);      // s bit on every request
    args.read("ECM", _ecm); // wrap in an ECM for the Map-Resolver
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;
    if (_eid_mask_len > 32)
	return errh->error("MASK_LEN must be 0-32");

    return 0;
}

/********************************************************************************************************************/

int
LISPMapRequester::initialize(ErrorHandler *)
{
    return 0;
}

/********************************************************************************************************************/

/********************************************************************************************************************/

Packet *
LISPMapRequester::make_request(IPAddress dst_eid, IPAddress src_eid,
    uint16_t *udpsport, uint32_t *nonce1, uint32_t *nonce2, bool smr_invoked)
{
    WritablePacket *q = Packet::make(
	sizeof(click_ip) + sizeof(click_udp) + sizeof(click_lisp_map_request));
    if (!q) {
	click_chatter("%p{element}: can't make Map-Request packet", this);
	return 0;
    }
    memset(q->data(), 0, q->length()); // Packet::make() does not zero memory

    click_ip *iph = reinterpret_cast<click_ip *>(q->data());
    click_udp *udph = reinterpret_cast<click_udp *>(iph + 1);
    click_lisp_map_request *mrh
	= reinterpret_cast<click_lisp_map_request *>(udph + 1);

    //***** IP and UDP headers (to the Map-Resolver: DST, or DST_ANNO)
    *udpsport = click_random();
    _hdr.fill(q, IPAddress(), *udpsport, htons(LISP_CONTROL_PORT));
    _version = iph->ip_v;
    uint16_t len = q->length() - sizeof(click_ip);

    // set lisp message
    uint32_t w = (uint32_t)LISP_MAP_REQUEST << LISP_CTRL_TYPE_SHIFT;
    if (_aut_bit_set)
	w |= LISP_MREQ_F_A;
    if (_reply_rec_includ)
	w |= LISP_MREQ_F_M;
    if (_is_pitr)
	w |= LISP_MREQ_F_PITR;
    if (_smr_rply || smr_invoked)
	w |= LISP_MREQ_F_SMR_INV;
    w |= 1; // IRC = 0 (one ITR-RLOC), one record
    mrh->type_flags = htonl(w);

    mrh->nonce1 = *nonce1 = click_random();
    mrh->nonce2 = *nonce2 = click_random();

    mrh->source_eid_afi = htons(LISP_AFI_IP);
    map_rqst_set_source_eid(mrh, src_eid);

    mrh->itr_rloc_afi = htons(LISP_AFI_IP);
    mrh->itr_rloc = iph->ip_src;

    mrh->rec_reserved = 0;
    mrh->rec_eid_mask_len = _eid_mask_len;
    mrh->rec_eid_afi = htons(LISP_AFI_IP);
    mrh->rec_eid_prefix = dst_eid;

    LISPIPHeader::finish_udp(q); // UDP checksum last

    if (_ecm) {
	// RFC 9301 section 5.8: outer IP/UDP to the Map-Resolver, ECM header,
	// then the Map-Request in an inner IP/UDP packet addressed to the EID.
	iph->ip_dst = dst_eid.in_addr();
	iph->ip_sum = 0;
	iph->ip_sum = click_in_cksum((unsigned char *)iph, sizeof(click_ip));
	udph->uh_sum = 0;
	unsigned icsum = click_in_cksum((unsigned char *)udph, len);
	udph->uh_sum = click_in_cksum_pseudohdr(icsum, iph, len);
	uint16_t sport = udph->uh_sport;

	WritablePacket *w = q->push(
	    sizeof(click_ip) + sizeof(click_udp) + sizeof(click_lisp_ecm));
	if (!w)
	    return 0;
	q = w;
	memset(q->data(), 0,
	    sizeof(click_ip) + sizeof(click_udp) + sizeof(click_lisp_ecm));
	click_lisp_ecm *ecm = reinterpret_cast<click_lisp_ecm *>(
	    q->data() + LISPIPHeader::headers);
	_hdr.fill(q, IPAddress(), sport,
	    htons(LISP_CONTROL_PORT)); // outer: to the Map-Resolver
	ecm->type_flags = htonl((uint32_t)LISP_ECM << LISP_CTRL_TYPE_SHIFT);
	LISPIPHeader::finish_udp(q);
    }
    return (q);
}

/*********************************************************************************************/

void
LISPMapRequester::handle_data(Packet *p)
{
    WritablePacket *q;
    uint32_t nonce1, nonce2;
    uint16_t port;
    IPAddress dst_rloc;

    const click_ip *iph = lisp_ip_header(p);

    IPAddress dst_eid = iph->ip_dst;
    IPAddress src_eid = iph->ip_src;

    uint32_t flow = lisp_flow_hash(
	iph, p->end_data() - reinterpret_cast<const uint8_t *>(iph));
    int rslt = _cache->lookup(dst_eid, &dst_rloc, flow);
    if (rslt == VALID_REPLY) {
	q = p->uniqueify();
	q->set_dst_ip_anno(dst_rloc);
	++_lookup_hit;
	output(0).push(q);
    } else if (rslt == NATIVE_FWD) {
	// negative Map-Reply: forward without LISP on output 2 (dropped if unconnected)
	++_native_fwd;
	checked_output_push(2, p);
    } else {
	++_lookup_miss;
	if (rslt == NOT_EXIST) {
	    q = (WritablePacket *)make_request(
		dst_eid, src_eid, &port, &nonce1, &nonce2);
	    if (!q) {
		click_chatter(
		    "%p{element}: can't make Map-Request packet", this);
		p->kill();
		return;
	    }
	    int rslt_in = _cache->insert(dst_eid, port, nonce1, nonce2);
	    if (!rslt_in) {
		// the cache can't track this request: drop both packets
		click_chatter(
		    "%p{element}: map-cache full, dropping packet", this);
		q->kill();
		p->kill();
		return;
	    }
	    _cache->queue_packet(dst_eid, p);
	    ++_rqst_sent;
	    output(1).push(q);
	} else
	    _cache->queue_packet(dst_eid, p);
    }

    return;
}

/********************************************************************************************************************/

// Returns the record of a valid Map-Reply that answers a pending request,
// or 0.  Checks the lengths of the header and of every record.
const Record *
LISPMapRequester::check_reply(Packet *p)
{
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    click_lisp_map_reply *rply_msg = (click_lisp_map_reply *)(udph + 1);
    const uint8_t *end = p->end_data();
    if (reinterpret_cast<const uint8_t *>(rply_msg + 1) > end) {
	click_chatter("%p{element}: truncated Map-Reply", this);
	return 0;
    }
    unsigned count = LISP_CTRL_COUNT(ntohl(rply_msg->type_flags));
    const uint8_t *r = reinterpret_cast<const uint8_t *>(rply_msg + 1);
    const Record *match = 0;
    uint16_t port = 0;
    for (unsigned i = 0; i < count; ++i) {
	const Record *rec = reinterpret_cast<const Record *>(r);
	if (r + 16 > end || r + lisp_record_size(rec) > end) {
	    click_chatter("%p{element}: truncated record in Map-Reply", this);
	    return 0;
	}
	// the pending request is found by nonce; the record's prefix must cover it
	if (!match
	    && _cache->reply_check(rec->eid_prefix, rec->mask_len,
		rply_msg->nonce1, rply_msg->nonce2, &port))
	    match = rec;
	r += lisp_record_size(rec);
    }
    if (!match) {
	click_chatter(
	    "%p{element}: no pending request matches this Map-Reply", this);
	return 0;
    }

    if ((udph->uh_sport != htons(LISP_CONTROL_PORT))
	|| (udph->uh_dport != port)) {
	click_chatter(
	    "%p{element}: Map-Reply ports %d -> %d do not match the request (%d -> %d)",
	    this, ntohs(udph->uh_sport), ntohs(udph->uh_dport),
	    LISP_CONTROL_PORT, ntohs(port));
	return 0;
    }
    if (!lisp_record_ttl(match)) {
	click_chatter("%p{element}: Map-Reply record TTL is 0", this);
	return 0;
    }
    return match;
}

/********************************************************************************************************************/

void
LISPMapRequester::handle_response(Packet *p)
{
    ++_all_resp;
    Packet *list;
    IPAddress locator;

    const Record *rec = check_reply(p);
    if (rec) {
	++_valid_resp;
	const click_lisp_map_reply *rh
	    = reinterpret_cast<const click_lisp_map_reply *>(
		lisp_udp_header(lisp_ip_header(p)) + 1);
	list = _cache->insert_response(rec, rh->nonce1, rh->nonce2, &locator);

	while (list) {
	    Packet *q = list->next();
	    list->set_next(0); // no longer in the queue
	    if (locator) {
		list->set_dst_ip_anno(locator);
		output(0).push(list);
	    } else {
		++_native_fwd; // negative mapping
		checked_output_push(2, list);
	    }
	    list = q;
	}
    }
}

/********************************************************************************************************************/

// SMR (RFC 9301 section 6.6.2): the ETR says our mapping for an EID is
// stale.  Keep using it, and send a Map-Request with the SMR-invoked bit;
// the reply refreshes the entry.  Only SMRs from a locator of the cached
// mapping are accepted, so other hosts cannot flush the cache.
void
LISPMapRequester::handle_smr(Packet *p)
{
    ++_smr_count;
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    click_lisp_map_request *mrh = (click_lisp_map_request *)(udph + 1);
    if (reinterpret_cast<const uint8_t *>(mrh + 1) > p->end_data()) {
	++_smr_ignored;
	return;
    }
    IPAddress eid = mrh->rec_eid_prefix;
    if (!_cache->has_locator(eid, iph->ip_src)) {
	++_smr_ignored;
	click_chatter(
	    "%p{element}: SMR for %s from %s ignored (not a locator of the mapping)",
	    this, eid.unparse().c_str(),
	    IPAddress(iph->ip_src).unparse().c_str());
	return;
    }
    uint16_t port;
    uint32_t nonce1, nonce2;
    Packet *q = make_request(eid, IPAddress(), &port, &nonce1, &nonce2, true);
    if (!q)
	return;
    if (!_cache->insert(eid, port, nonce1, nonce2)) {
	q->kill();
	return;
    }
    ++_rqst_sent;
    output(1).push(q);
}

/********************************************************************************************************************/

void
LISPMapRequester::push(int port, Packet *p)
{
    if (port == 0)
	handle_data(p);
    else {
	if (port == 1)
	    handle_response(p);
	else
	    handle_smr(p);
	p->kill();
    }
}

/********************************************************************************************************************/

String
LISPMapRequester::read_handler(Element *e, void *thunk)
{
    LISPMapRequester *q = (LISPMapRequester *)e;
    switch (reinterpret_cast<uintptr_t>(thunk)) {
    case h_cache: {
	return q->_cache->read_handler(
	    q->_cache, (void *)(uintptr_t)LISPMapCache::h_cache);
	break;
    }
    case h_stats: {
	return String(q->_lookup_hit.value()) + " lookup hits\n"
	    + String(q->_lookup_miss.value()) + " lookup misses\n"
	    + String(q->_rqst_sent.value()) + " Map-Requests sent\n"
	    + String(q->_all_resp.value()) + " Map-Replies received\n"
	    + String(q->_valid_resp.value()) + " Map-Replies valid\n";
	break;
    }
    case h_count: {
	return String(q->_cache->count());
	break;
    }
    case h_length: {
	return String(q->_cache->length());
	break;
    }
    default: {
	return String();
	break;
    }
    }
}

/********************************************************************************************************************/

/********************************************************************************************************************/

void
LISPMapRequester::add_handlers()
{
    add_read_handler("table", read_handler, h_cache);
    add_read_handler("stats", read_handler, h_stats);
    add_read_handler("count", read_handler, h_count);
    add_read_handler("length", read_handler, h_length);
    add_data_handlers("smr count", Handler::OP_READ, &_smr_count);
    add_data_handlers(
	"smrs", Handler::OP_READ, &_smr_count); // Click-style name
    add_data_handlers("smr ignored", Handler::OP_READ, &_smr_ignored);
    add_data_handlers(
	"smrs_ignored", Handler::OP_READ, &_smr_ignored); // Click-style name
    add_data_handlers("native forwarded", Handler::OP_READ, &_native_fwd);
    add_data_handlers(
	"native_forwarded", Handler::OP_READ, &_native_fwd); // Click-style name
    add_data_handlers("protocol version", Handler::OP_READ, &_version);
    add_data_handlers(
	"ip_version", Handler::OP_READ, &_version); // Click-style name
}

/********************************************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapRequester)
ELEMENT_REQUIRES(LISPMapCache LISPCommon)
ELEMENT_MT_SAFE(LISPMapRequester)

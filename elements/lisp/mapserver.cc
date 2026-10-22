/*
 * mapserver.{cc,hh} -- Map-Server element: Map-Register, Map-Notify (LISPMapServer)
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
#include "mapserver.hh"
#include <click/args.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "lispcommon.hh"
#include <clicknet/udp.h>
#include <clicknet/ip.h>

CLICK_DECLS

/******************************************************************************************************************/

LISPMapServer::LISPMapServer()
  : _mbase(0)
{
    _rgstmsg_received = _notifmsg_sent = 0;
    _valid_rgstmsg = 0;
    _dp = _sp = _type = _ttl = 0;
    _auth_failures = 0;
}

/******************************************************************************************************************/

LISPMapServer::~LISPMapServer()
{
}

/******************************************************************************************************************/

int
LISPMapServer::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Vector<String> site_strs;
    Args args(conf, this, errh);
    args.read_mp("MBase", ElementCastArg("LISPMappingDB"), _mbase);
    args.read_all("SITE", AnyArg(), site_strs);
    _hdr.read_args(args); // PROTO, SRC, DST and the IP keywords
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;

    for (int i = 0; i < site_strs.size(); ++i) {
	Vector<String> w;
	cp_spacevec(cp_unquote(site_strs[i]), w);
	IPAddress a, m;
	if (w.size() != 2 || !IPPrefixArg(true).parse(w[0], a, m, this)
	    || m.mask_to_prefix_len() < 0)
	    return errh->error("SITE must be %<PREFIX KEY%>");
	Site s;
	s.prefix = LISPPrefix(a, m.mask_to_prefix_len());
	s.key = w[1];
	_sites.push_back(s);
    }

    return 0;
}

/*****************************************************************************************************************/

int
LISPMapServer::initialize(ErrorHandler *)
{
    return 0;
}

/*****************************************************************************************************************/

/*****************************************************************************************************************/

// Site that contains the prefix eid/len, or 0.
const LISPMapServer::Site *
LISPMapServer::find_site(IPAddress eid, int len) const
{
    const Site *best = 0;
    for (int i = 0; i < _sites.size(); ++i)
	if (_sites[i].prefix.len <= len && _sites[i].prefix.contains(eid)
	    && (!best || _sites[i].prefix.len > best->prefix.len))
	    best = &_sites[i];
    return best;
}

// Map-Notify answering a Map-Register (RFC 9301 section 5.7): a new
// packet with the same nonce, Key ID and records, signed with the site's
// key, sent to the ETR's source address and port.
Packet *
LISPMapServer::make_notify(Packet *rgst_p, const String &key)
{
    const click_ip *in_iph = lisp_ip_header(rgst_p);
    const click_udp *in_udph = lisp_udp_header(in_iph);
    const uint8_t *msg = reinterpret_cast<const uint8_t *>(in_udph + 1);
    size_t mlen = rgst_p->end_data() - msg;
    WritablePacket *q
	= Packet::make(sizeof(click_ip) + sizeof(click_udp) + mlen);
    if (!q) {
	click_chatter("%p{element}: can't make Map-Notify packet", this);
	return 0;
    }
    memset(q->data(), 0, sizeof(click_ip) + sizeof(click_udp));
    click_ip *iph = reinterpret_cast<click_ip *>(q->data());
    click_udp *udph = reinterpret_cast<click_udp *>(iph + 1);
    click_lisp_map_notify *notif_msg
	= reinterpret_cast<click_lisp_map_notify *>(udph + 1);
    memcpy(notif_msg, msg, mlen);

    // IP/UDP: to the ETR, back to its source port
    _hdr.fill(q, in_iph->ip_src, htons(LISP_CONTROL_PORT), in_udph->uh_sport);

    //**SET THE LISP-HEADER
    notif_msg->type_flags = htonl(
	((uint32_t)LISP_MAP_NOTIFY << LISP_CTRL_TYPE_SHIFT)
	| LISP_CTRL_COUNT(ntohl(notif_msg->type_flags))); // keep record count
    if (key
	&& ntohs(notif_msg->auth_data_len) == LISP_AUTH_LEN_HMAC_SHA_256_128)
	lisp_auth_sign(reinterpret_cast<uint8_t *>(notif_msg), mlen,
	    sizeof(click_lisp_map_notify), key);

    LISPIPHeader::finish_udp(q); // UDP checksum last
    return q;
}

/*********************************************************************************************/

void
LISPMapServer::handle_register(Packet *p)
{
    _rgstmsg_received++;
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    click_lisp_map_register *mrgst_msg = (click_lisp_map_register *)(udph + 1);
    const uint8_t *msg = reinterpret_cast<const uint8_t *>(mrgst_msg);
    size_t mlen = p->end_data() > msg ? p->end_data() - msg : 0;
    uint16_t key_id = mlen >= sizeof(click_lisp_map_register)
	? ntohs(mrgst_msg->key_id)
	: 0;
    uint16_t aut_len = mlen >= sizeof(click_lisp_map_register)
	? ntohs(mrgst_msg->auth_data_len)
	: 0;
    if (mlen < sizeof(click_lisp_map_register) + aut_len + 16) {
	click_chatter("%p{element}: truncated Map-Register", this);
	return;
    }
    // records follow the authentication data
    Record *rec1 = (Record *)(msg + sizeof(click_lisp_map_register) + aut_len);
    uint8_t rec_cnt = LISP_CTRL_COUNT(ntohl(mrgst_msg->type_flags));

    // Authentication (RFC 9301 section 5.6): with SITE configured, the
    // HMAC must verify with the site's key and every registered prefix
    // must be inside a site that has this key.
    const Site *site = 0;
    if (_sites.size()) {
	site = find_site(rec1->eid_prefix, rec1->mask_len);
	bool ok = site && key_id == LISP_KEY_ID_HMAC_SHA_256_128
	    && aut_len == LISP_AUTH_LEN_HMAC_SHA_256_128
	    && lisp_auth_verify(
		msg, mlen, sizeof(click_lisp_map_register), site->key);
	const uint8_t *r = reinterpret_cast<const uint8_t *>(rec1);
	for (unsigned i = 0; ok && i < rec_cnt; ++i) {
	    const Record *rec = reinterpret_cast<const Record *>(r);
	    if (r + 16 > p->end_data()
		|| r + lisp_record_size(rec) > p->end_data())
		ok = false;
	    else {
		const Site *s = find_site(rec->eid_prefix, rec->mask_len);
		ok = s && s->key == site->key;
		r += lisp_record_size(rec);
	    }
	}
	if (!ok) {
	    ++_auth_failures;
	    click_chatter(
		"%p{element}: Map-Register from %s rejected (authentication or site)",
		this, IPAddress(iph->ip_src).unparse().c_str());
	    return;
	}
    }

    IPAddress etr_rloc = iph->ip_src;
    uint16_t version = lisp_record_version(rec1);
    int indice;
    int rslt = _mbase->ms_lookup(etr_rloc, version, &indice);

    if (rslt == EQL)
	return;

    if (ntohl(mrgst_msg->type_flags) & LISP_MREG_F_WANT_NOTIFY) {
	WritablePacket *q
	    = (WritablePacket *)make_notify(p, site ? site->key : String());
	if (!q) {
	    click_chatter("%p{element}: can't make Map-Notify packet", this);
	    return;
	}

	//statistiques
	_dest = iph->ip_dst;
	_src = iph->ip_src;
	_sp = ntohs(udph->uh_sport);
	_dp = ntohs(udph->uh_dport);

	_type = LISP_CTRL_TYPE(ntohl(mrgst_msg->type_flags));
	_ttl = iph->ip_ttl;
	output(0).push(q);
	++_notifmsg_sent;
    }

    ++_valid_rgstmsg;

    _mbase->ms_register(rec_cnt, rec1, indice, p->end_data());
}

/******************************************************************************************************************/

void
LISPMapServer::push(int port, Packet *p)
{
    if (!port)
	handle_register(p);
    p->kill();
}

/******************************************************************************************************************/

void
LISPMapServer::add_handlers()
{
    add_data_handlers("rgstr_received", Handler::OP_READ, &_rgstmsg_received);
    add_data_handlers(
	"registers", Handler::OP_READ, &_rgstmsg_received); // Click-style name
    add_data_handlers("notif_sent", Handler::OP_READ, &_notifmsg_sent);
    add_data_handlers(
	"notifies", Handler::OP_READ, &_notifmsg_sent); // Click-style name
    add_data_handlers("auth_failures", Handler::OP_READ, &_auth_failures);
    //statistiques
    add_data_handlers("addsrc", Handler::OP_READ, &_src);
    add_data_handlers("last_src", Handler::OP_READ, &_src); // Click-style name
    add_data_handlers("adddest", Handler::OP_READ, &_dest);
    add_data_handlers("last_dst", Handler::OP_READ, &_dest); // Click-style name
    add_data_handlers("srcp", Handler::OP_READ, &_sp);
    add_data_handlers("last_sport", Handler::OP_READ, &_sp); // Click-style name
    add_data_handlers("dstp", Handler::OP_READ, &_dp);
    add_data_handlers("last_dport", Handler::OP_READ, &_dp); // Click-style name
    add_data_handlers("version", Handler::OP_READ, &_type);
    add_data_handlers(
	"last_type", Handler::OP_READ, &_type); // Click-style name
    add_data_handlers("ttl", Handler::OP_READ, &_ttl);
    add_data_handlers("last_ttl", Handler::OP_READ, &_ttl); // Click-style name
}

/******************************************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapServer)
ELEMENT_REQUIRES(LISPMappingDB LISPCommon)
ELEMENT_MT_SAFE(LISPMapServer)

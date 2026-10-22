/*
 * register.{cc,hh} -- ETR element sending Map-Register and checking Map-Notify
 * (LISPMapRegistrar)
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
#include <click/ipaddress.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/udp.h>
#include <clicknet/ip.h>
#include "lispcommon.hh"
#include "register.hh"

CLICK_DECLS

LISPMapRegistrar::LISPMapRegistrar()
  : _reg_timer(this)
{
    _interval = STABLE_P;
    _retry = CHANGE_P;
    _confirmed = false;
    _last_nonce1 = _last_nonce2 = 0;
    _notif_valid = 0;
    _notif_received = _register_sent = 0;
}

LISPMapRegistrar::~LISPMapRegistrar()
{
}

int
LISPMapRegistrar::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    args.read_mp("BASE", ElementCastArg("LISPEtrDatabase"), _base);
    _hdr.read_args(args);   // PROTO, SRC, DST and the IP keywords
    args.read("KEY", _key); // HMAC-SHA-256-128 key shared with the Map-Server
    args.read("INTERVAL", _interval); // seconds, RFC 9301 recommends 60
    args.read("RETRY", _retry);       // seconds, until a Map-Notify confirms
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;

    return 0;
};

void
LISPMapRegistrar::run_timer(Timer *)
{
    send_register();
    // until a Map-Notify confirms the registration, retry sooner
    _reg_timer.reschedule_after_sec(_confirmed ? _interval : _retry);
}

int
LISPMapRegistrar::initialize(ErrorHandler *)
{
    _reg_timer.initialize(
	this); // timers belong in initialize(), not configure()
    _reg_timer.schedule_now();
    return 0;
}

void
LISPMapRegistrar::send_register()
{
    ++_register_sent;
    uint8_t rec_count;
    WritablePacket *p = (WritablePacket *)_base->register_records(&rec_count);

    if (!p) {
	click_chatter("%p{element}: can't make Map-Register packet", this);
	return;
    }

    // records are already in p; push the headers and the authentication field
    uint16_t aut_len = _key ? LISP_AUTH_LEN_HMAC_SHA_256_128 : 0;
    WritablePacket *q = p->push(
	LISPIPHeader::headers + sizeof(click_lisp_map_register) + aut_len);
    if (!q)
	return;
    memset(q->data(), 0,
	LISPIPHeader::headers + sizeof(click_lisp_map_register)
	    + aut_len); // headers pushed on uninitialized headroom
    click_lisp_map_register *reg_h
	= reinterpret_cast<click_lisp_map_register *>(
	    q->data() + LISPIPHeader::headers);

    // IP/UDP: to the Map-Server (DST, or DST_ANNO)
    _hdr.fill(q, IPAddress(), click_random(), htons(LISP_CONTROL_PORT));

    // set lisp message

    reg_h->type_flags
	= htonl(((uint32_t)LISP_MAP_REGISTER << LISP_CTRL_TYPE_SHIFT)
	    | LISP_MREG_F_WANT_NOTIFY | rec_count); // P=0, M=1
    _confirmed = false;
    reg_h->nonce1 = _last_nonce1 = click_random();
    reg_h->nonce2 = _last_nonce2 = click_random();
    reg_h->key_id
	= htons(_key ? LISP_KEY_ID_HMAC_SHA_256_128 : LISP_KEY_ID_NONE);
    reg_h->auth_data_len = htons(aut_len);
    if (_key) // HMAC over the whole message (header, auth field = 0, records)
	lisp_auth_sign(reinterpret_cast<uint8_t *>(reg_h),
	    q->end_data() - reinterpret_cast<uint8_t *>(reg_h),
	    sizeof(click_lisp_map_register), _key);

    LISPIPHeader::finish_udp(q); // UDP checksum last
    output(0).push(q);
}

void
LISPMapRegistrar::handle_notify(Packet *p)
{
    ++_notif_received;
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    click_lisp_map_notify *notif_h = (click_lisp_map_notify *)(udph + 1);

    // A Map-Notify confirms our last Map-Register: it must echo its nonce
    // and, when a key is configured, carry a valid HMAC.
    const uint8_t *msg = reinterpret_cast<const uint8_t *>(notif_h);
    size_t len = p->end_data() - msg;
    if (len < sizeof(click_lisp_map_notify) || notif_h->nonce1 != _last_nonce1
	|| notif_h->nonce2 != _last_nonce2) {
	click_chatter(
	    "%p{element}: Map-Notify ignored (no matching Map-Register)", this);
	return;
    }
    if (_key) {
	if (ntohs(notif_h->key_id) != LISP_KEY_ID_HMAC_SHA_256_128
	    || ntohs(notif_h->auth_data_len) != LISP_AUTH_LEN_HMAC_SHA_256_128
	    || !lisp_auth_verify(
		msg, len, sizeof(click_lisp_map_notify), _key)) {
	    click_chatter(
		"%p{element}: Map-Notify ignored (bad authentication)", this);
	    return;
	}
    }
    ++_notif_valid;
    _confirmed = true;
}

void
LISPMapRegistrar::push(int port, Packet *p)
{
    if (!port)
	handle_notify(p);
    p->kill();
}

void
LISPMapRegistrar::add_handlers()
{
    add_data_handlers("m_notif count", Handler::OP_READ, &_notif_received);
    add_data_handlers(
	"notifies", Handler::OP_READ, &_notif_received); // Click-style name
    add_data_handlers("m_rgst count", Handler::OP_READ, &_register_sent);
    add_data_handlers(
	"registers", Handler::OP_READ, &_register_sent); // Click-style name
    add_data_handlers("m_notif valid", Handler::OP_READ, &_notif_valid);
    add_data_handlers(
	"notifies_valid", Handler::OP_READ, &_notif_valid); // Click-style name
    add_write_handler("register", write_handler, 0);
}

// "register": send a Map-Register now (for LISPMigrationUpdater, scripts
// and tests)
int
LISPMapRegistrar::write_handler(
    const String &, Element *e, void *, ErrorHandler *)
{
    static_cast<LISPMapRegistrar *>(e)->send_register();
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapRegistrar)
ELEMENT_REQUIRES(LISPEtrDatabase LISPCommon)
ELEMENT_MT_SAFE(LISPMapRegistrar)

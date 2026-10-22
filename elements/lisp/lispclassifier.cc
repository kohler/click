/*
 * lispclassifier.{cc,hh} -- elements classify LISP data and control packets for an
 * xTR, a Map-Server and the FMCC controller
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
#include "lispclassifier.hh"
#include <click/args.hh>
#include <click/error.hh>
#include "lispcommon.hh"
#include <clicknet/ip.h>
#include <clicknet/udp.h>
CLICK_DECLS

LISPClassifierBase::LISPClassifierBase()
  : _value_type(0),
    _mapnotify(0),
    _reserv(0),
    _info(0),
    _smr(0),
    _udp_sp(0),
    _udp_dp(0),
    _verbose(false)
{
    _request_received = _register_received = 0;
}

int
LISPClassifierBase::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read("VERBOSE", _verbose).complete();
}

LISPClassifierBase::Kind
LISPClassifierBase::control_kind(const uint32_t *msg)
{
    if (!msg)
	return K_TOO_SHORT;
    _info = *msg;
    uint32_t w = ntohl(*msg); // first word, host order
    _value_type = LISP_CTRL_TYPE(w);
    _smr = (w & LISP_MREQ_F_S) ? 1 : 0;
    _mapnotify = (w & LISP_MREG_F_WANT_NOTIFY) ? 1 : 0;
    _reserv = w & lisp_ctrl_reserved_mask(_value_type);
    if (_reserv)
	return K_RESERVED;
    switch (_value_type) {
    case LISP_MAP_REQUEST:
	++_request_received;
	return _smr ? K_SMR : K_MAP_REQUEST;
    case LISP_MAP_REPLY:
	return K_MAP_REPLY;
    case LISP_MAP_REGISTER:
	++_register_received;
	return K_MAP_REGISTER;
    case LISP_MAP_NOTIFY:
	return K_MAP_NOTIFY;
    case LISP_ECM:
	return K_ECM;
    case LISP_SHARED_EXTENSION:
	return LISP_CTRL_SUBTYPE(w) == LISP_FMCC_MIGRATE_SUBTYPE ? K_MAP_MIGRATE
								 : K_UNKNOWN;
    default:
	return K_UNKNOWN;
    }
}

void
LISPClassifierBase::push(int, Packet *p)
{
    static const char *const names[] = {"not LISP", "LISP data",
	"control message too short", "control message with reserved bits set",
	"Map-Request", "SMR", "Map-Reply", "Map-Register", "Map-Notify",
	"Map-Migrate", "ECM", "unknown control message"};
    // Never writes into the packet.
    const click_ip *iph = lisp_ip_header(static_cast<const Packet *>(p));
    const click_udp *udph = lisp_udp_header(iph);
    const uint32_t *msg = reinterpret_cast<const uint32_t *>(udph + 1);
    const uint8_t *end = p->end_data();
    bool is_data = false, is_ctrl = false;
    Kind kind = K_NOT_LISP;

    // only UDP packets long enough for a UDP header can be LISP
    if (reinterpret_cast<const uint8_t *>(iph) + sizeof(click_ip) <= end
	&& iph->ip_p == IP_PROTO_UDP
	&& reinterpret_cast<const uint8_t *>(udph + 1) <= end) {
	_udp_sp = ntohs(udph->uh_sport);
	_udp_dp = ntohs(udph->uh_dport);
	is_data = _udp_sp == LISP_DATA_PORT || _udp_dp == LISP_DATA_PORT;
	is_ctrl = _udp_sp == LISP_CONTROL_PORT || _udp_dp == LISP_CONTROL_PORT;
	if (reinterpret_cast<const uint8_t *>(msg + 1) > end)
	    msg = 0;
	if (is_ctrl)
	    kind = control_kind(msg);
	else if (is_data)
	    kind = K_DATA;
    }

    int port = choose(is_data, is_ctrl, kind);
    if (_verbose)
	click_chatter("%p{element}: %s -> output %d", this, names[kind], port);
    checked_output_push(port, p);
}

void
LISPClassifierBase::add_common_handlers()
{
    add_data_handlers("Request received", Handler::OP_READ, &_request_received);
    add_data_handlers(
	"requests", Handler::OP_READ, &_request_received); // Click-style name
    add_data_handlers(
	"Register received", Handler::OP_READ, &_register_received);
    add_data_handlers(
	"registers", Handler::OP_READ, &_register_received); // Click-style name
    add_data_handlers("type", Handler::OP_READ, &_value_type);
    add_data_handlers("want notify", Handler::OP_READ, &_mapnotify);
    add_data_handlers(
	"want_notify", Handler::OP_READ, &_mapnotify); // Click-style name
    add_data_handlers("reserved field", Handler::OP_READ, &_reserv);
    add_data_handlers(
	"reserved", Handler::OP_READ, &_reserv); // Click-style name
    add_data_handlers("informations", Handler::OP_READ, &_info);
    add_data_handlers(
	"last_word", Handler::OP_READ, &_info); // Click-style name
}

/* ---------------- xTR ----------------
   0 plain IP (to encapsulate)   1 Map-Request   2 Map-Reply   3 Map-Notify
   4 Map-Migrate   5 LISP data (to decapsulate)   6 error/unknown   7 SMR   */
int
LISPClassifierxTR::choose(bool is_data, bool is_ctrl, Kind kind)
{
    if (is_data)
	return 5;
    if (!is_ctrl)
	return 0;
    switch (kind) {
    case K_MAP_REQUEST:
	return 1;
    case K_SMR:
	return 7;
    case K_MAP_REPLY:
	return 2;
    case K_MAP_NOTIFY:
	return 3;
    case K_MAP_MIGRATE:
	return 4;
    default:
	return 6;
    }
}

void
LISPClassifierxTR::add_handlers()
{
    add_common_handlers();
    add_data_handlers("Source Port", Handler::OP_READ, &_udp_sp);
    add_data_handlers("sport", Handler::OP_READ, &_udp_sp); // Click-style name
    add_data_handlers("Dource Port", Handler::OP_READ, &_udp_dp);
    add_data_handlers("dport", Handler::OP_READ, &_udp_dp); // Click-style name
}

/* ---------------- Map-Server ----------------
   0 other/error   1 Map-Request (plain or in an ECM)   2 Map-Register   3 Map-Migrate */
int
LISPClassifierMS::choose(bool, bool is_ctrl, Kind kind)
{
    if (!is_ctrl)
	return 0;
    switch (kind) {
    case K_MAP_REQUEST:
    case K_SMR:
    case K_ECM:
	return 1; // ECM carries a Map-Request for the Map-Resolver
    case K_MAP_REGISTER:
	return 2;
    case K_MAP_MIGRATE:
	return 3;
    default:
	return 0;
    }
}

void
LISPClassifierMS::add_handlers()
{
    add_common_handlers();
    add_data_handlers("SMR", Handler::OP_READ, &_smr);
    add_data_handlers("smr", Handler::OP_READ, &_smr); // Click-style name
}

/* ---------------- FMCC ----------------
   0 LISP data   1 Map-Register   2 Map-Notify   3 other */
int
LISPClassifierFMCC::choose(bool is_data, bool is_ctrl, Kind kind)
{
    if (is_data)
	return 0;
    if (!is_ctrl)
	return 3;
    switch (kind) {
    case K_MAP_REGISTER:
	return 1;
    case K_MAP_NOTIFY:
	return 2;
    default:
	return 3;
    }
}

void
LISPClassifierFMCC::add_handlers()
{
    add_common_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPClassifierxTR)
EXPORT_ELEMENT(LISPClassifierMS)
EXPORT_ELEMENT(LISPClassifierFMCC)
ELEMENT_MT_SAFE(LISPClassifierxTR)
ELEMENT_MT_SAFE(LISPClassifierMS)
ELEMENT_MT_SAFE(LISPClassifierFMCC)

/*
 * etherpausesource.{cc,hh} -- generates Ethernet PAUSE MAC control frames
 * Eddie Kohler, based on etherpause.cc by Roman Chertov
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "etherpausesource.hh"
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

static const unsigned char default_destination[6] = {
    0x01, 0x80, 0xC2, 0x00, 0x00, 0x01
};

EtherPauseSource::EtherPauseSource()
    : _packet(0), _timer(this)
{
}

EtherPauseSource::~EtherPauseSource()
{
}

int
EtherPauseSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    EtherAddress src, dst(default_destination);
    uint16_t pausetime;
    _limit = -1;
    _active = true;
    _interval = 1000;
    if (Args(conf, this, errh)
	.read_mp("SRC", src)
	.read_mp("PAUSETIME", pausetime)
	.read("DST", dst)
	.read("LIMIT", _limit)
	.read("ACTIVE", _active)
	.read("INTERVAL", SecondsArg(3), _interval)
	.complete() < 0)
        return -1;

    // build PAUSE frame
    WritablePacket *q;
    if (!(q = Packet::make(64)))
	return errh->error("out of memory!"), -ENOMEM;

    q->set_mac_header(q->data(), sizeof(click_ether));
    click_ether *ethh = q->ether_header();
    memcpy(ethh->ether_dhost, &dst, 6);
    memcpy(ethh->ether_shost, &src, 6);
    ethh->ether_type = htons(ETHERTYPE_MACCONTROL);

    click_ether_macctl *emch = (click_ether_macctl *) q->network_header();
    emch->ether_macctl_opcode = htons(ETHER_MACCTL_OP_PAUSE);
    emch->ether_macctl_param = htons(pausetime);
    memset(emch->ether_macctl_reserved, 0, sizeof(emch->ether_macctl_reserved));

    _packet = q;
    return 0;
}

int
EtherPauseSource::initialize(ErrorHandler *)
{
    _count = 0;
    _timer.initialize(this);
    if (_limit != 0 && _active && output_is_push(0))
	_timer.schedule_after_msec(_interval);
    return 0;
}

void
EtherPauseSource::cleanup(CleanupStage)
{
    if (_packet) {
        _packet->kill();
        _packet = 0;
    }
}

void
EtherPauseSource::run_timer(Timer *)
{
    if (Packet *p = _packet->clone()) {
	++_count;
	output(0).push(p);
    }
    if (_limit < 0 || _count < _limit)
	_timer.reschedule_after_msec(_interval);
}

Packet *
EtherPauseSource::pull(int)
{
    if (!_active || (_limit >= 0 && _count >= _limit))
        return 0;
    if (Packet *p = _packet->clone()) {
	++_count;
	return p;
    } else
	return 0;
}

String
EtherPauseSource::reader(Element *e, void *user_data)
{
    EtherPauseSource *eps = static_cast<EtherPauseSource *>(e);
    switch ((intptr_t) user_data) {
    case h_src:
	return EtherAddress(eps->_packet->ether_header()->ether_shost).unparse();
    case h_dst:
	return EtherAddress(eps->_packet->ether_header()->ether_dhost).unparse();
    case h_pausetime: {
	const click_ether_macctl *emc = (const click_ether_macctl *) eps->_packet->network_header();
	return String(ntohs(emc->ether_macctl_param));
    }
    default:
	return String();
    }
}

int
EtherPauseSource::rewrite_packet(const void *data, uint32_t offset, uint32_t size, ErrorHandler *errh)
{
    if (WritablePacket *q = Packet::make(64)) {
	memcpy(q->data(), _packet->data(), 64);
	memcpy(q->data() + offset, data, size);
	q->set_mac_header(q->data(), sizeof(click_ether));
	_packet->kill();
	_packet = q;
	return 0;
    } else
	return errh->error("out of memory!"), -ENOMEM;
}

void
EtherPauseSource::check_awake()
{
    if (output_is_push(0) && !_timer.scheduled() && _active)
	_timer.schedule_now();
}

int
EtherPauseSource::writer(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    EtherPauseSource *eps = static_cast<EtherPauseSource *>(e);
    switch ((intptr_t) user_data) {
    case h_src:
    case h_dst: {
	EtherAddress a;
	if (!EtherAddressArg().parse(str, a, e))
	    return errh->error("type mismatch");
	return eps->rewrite_packet(&a, ((intptr_t) user_data == h_src ? offsetof(click_ether, ether_shost) : offsetof(click_ether, ether_dhost)), 6, errh);
    }
    case h_pausetime: {
	uint32_t x;
	if (!IntArg().parse(str, x) || x > 0xFFFF)
	    return errh->error("type mismatch");
	uint16_t param = htons((uint16_t) x);
	return eps->rewrite_packet(&param, sizeof(click_ether) + offsetof(click_ether_macctl, ether_macctl_param), 2, errh);
    }
    case h_limit:
	if (!IntArg().parse(str, eps->_limit))
	    return errh->error("type mismatch");
	eps->check_awake();
	return 0;
    case h_active:
	if (!BoolArg().parse(str, eps->_active))
	    return errh->error("type mismatch");
	eps->check_awake();
	return 0;
    case h_reset_counts:
	eps->_count = 0;
	eps->check_awake();
	return 0;
    default:
	return 0;
    }
}

void
EtherPauseSource::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("limit", Handler::OP_READ, &_limit);
    add_write_handler("limit", writer, h_limit);
    add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
    add_write_handler("active", writer, h_active);
    add_read_handler("src", reader, h_src);
    add_write_handler("src", writer, h_src);
    add_read_handler("dst", reader, h_dst);
    add_write_handler("dst", writer, h_dst);
    add_read_handler("pausetime", reader, h_pausetime);
    add_write_handler("pausetime", writer, h_pausetime);
    add_write_handler("reset_counts", writer, h_reset_counts);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherPauseSource)

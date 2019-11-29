/*
 * settxrateht.{cc,hh} -- sets wifi txrate annotation on a packet
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "settxrateht.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

SetTXRateHT::SetTXRateHT() : _mcs(-1), _mcs1(-1), _mcs2(-1), _mcs3(-1),
                         _max_tries(0), _max_tries1(0), _max_tries2(0),
                         _max_tries3(0), _et(0), _offset(0), _sgi(false),
                         _bw_40(false) {
}

SetTXRateHT::~SetTXRateHT() {
}

int SetTXRateHT::configure(Vector<String> &conf, ErrorHandler *errh) {
	return Args(conf, this, errh)
			.read_p("MCS", _mcs)
			.read("TRIES", _max_tries)
			.read("MCS1", _mcs1)
			.read("TRIES1", _max_tries1)
			.read("MCS2", _mcs2)
			.read("TRIES2", _max_tries2)
			.read("MCS3", _mcs3)
			.read("TRIES3", _max_tries3)
			.read("ETHTYPE", _et)
			.read("OFFSET", _offset)
			.read("SGI", _sgi)
			.read("BW_40", _bw_40)
			.complete();
}

Packet *
SetTXRateHT::simple_action(Packet *p_in) {

	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	click_ether *eh = (click_ether *) dst_ptr;

	if (_et && eh->ether_type != htons(_et)) {
		return p_in;
	}

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

	ceh->magic = WIFI_EXTRA_MAGIC;

	ceh->rate = _mcs;
	ceh->max_tries = _max_tries;

	ceh->rate1 = _mcs1;
	ceh->max_tries1 = _max_tries1;

	ceh->rate2 = _mcs2;
	ceh->max_tries2 = _max_tries2;

	ceh->rate3 = _mcs3;
	ceh->max_tries3 = _max_tries3;

	ceh->flags |= WIFI_EXTRA_MCS;

	if (_sgi)
		ceh->flags |= WIFI_EXTRA_MCS_SGI;

	if (_bw_40)
		ceh->flags |= WIFI_EXTRA_MCS_BW_40;

	return p_in;

}

enum {
	H_MCS, H_TRIES
};

String SetTXRateHT::read_handler(Element *e, void *thunk) {
	SetTXRateHT *foo = (SetTXRateHT *) e;
	switch ((uintptr_t) thunk) {
	case H_MCS:
		return String(foo->_mcs) + "\n";
	case H_TRIES:
		return String(foo->_max_tries) + "\n";
	default:
		return "\n";
	}
}

int SetTXRateHT::write_handler(const String &arg, Element *e, void *vparam,
		ErrorHandler *errh) {
	SetTXRateHT *f = (SetTXRateHT *) e;
	String s = cp_uncomment(arg);
	switch ((intptr_t) vparam) {
	case H_MCS: {
		unsigned m;
		if (!IntArg().parse(s, m))
			return errh->error("rate parameter must be unsigned");
		f->_mcs = m;
		break;
	}
	case H_TRIES: {
		unsigned m;
		if (!IntArg().parse(s, m))
			return errh->error("tries parameter must be unsigned");
		f->_max_tries = m;
		break;
	}
	}
	return 0;
}

void SetTXRateHT::add_handlers() {
	add_read_handler("rate", read_handler, H_MCS);
	add_read_handler("tries", read_handler, H_TRIES);
	add_write_handler("rate", write_handler, H_MCS);
	add_write_handler("tries", write_handler, H_TRIES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRateHT)

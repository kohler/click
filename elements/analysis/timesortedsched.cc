// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * timesortedsched.{cc,hh} -- element merges sorted packet streams by timestamp
 * Eddie Kohler
 *
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2010-2011 Regents of the University of California
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
#include <click/error.hh>
#include "timesortedsched.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/heap.hh>
CLICK_DECLS

TimeSortedSched::TimeSortedSched()
    : _pkt(0), _npkt(0), _input(0), _nready(0),
      _notifier(Notifier::SEARCH_CONTINUE_WAKE), _buffer(1),
      _well_ordered(true)
{
}

TimeSortedSched::~TimeSortedSched()
{
}

void *
TimeSortedSched::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return &_notifier;
    else
	return Element::cast(n);
}

int
TimeSortedSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    _stop = false;
    if (Args(conf, this, errh)
	.read("STOP", _stop)
	.read("BUFFER", _buffer)
	.complete() < 0)
	return -1;
    if (_buffer <= 0)
	return errh->error("BUFFER must be at least 1");
    return 0;
}

int
TimeSortedSched::initialize(ErrorHandler *errh)
{
    _pkt = new packet_s[ninputs() * _buffer];
    _input = new input_s[ninputs()];
    if (!_pkt || !_input)
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++) {
	_input[i].signal = Notifier::upstream_empty_signal(this, i, &_notifier);
	_input[i].space = _buffer;
	_input[i].ready = i;
    }
    _nready = ninputs();
    return 0;
}

void
TimeSortedSched::cleanup(CleanupStage)
{
    for (int i = 0; i < _npkt; ++i)
	_pkt[i].p->kill();
    delete[] _pkt;
    delete[] _input;
}

Packet*
TimeSortedSched::pull(int)
{
    bool signals_on = false;
    // first maybe fill in buffer
    for (int rpos = _nready - 1; rpos >= 0; --rpos) {
	int i = _input[rpos].ready;
	input_s &is = _input[i];
	if (is.signal) {
	    signals_on = true;
	    while ((_pkt[_npkt].p = input(i).pull())) {
		_pkt[_npkt].input = i;
		++_npkt;
		push_heap(_pkt, _pkt + _npkt, heap_less());
		--is.space;
		if (!is.space) {
		    _input[rpos].ready = _input[_nready - 1].ready;
		    --_nready;
		    break;
		}
	    }
	}
    }

    // then maybe emit a packet
    _notifier.set_active(_npkt > 0 || signals_on);
    if (_npkt > 0) {
	Packet *p = _pkt[0].p;
	if (p->timestamp_anno()) {
	    if (_last_emission && p->timestamp_anno() < _last_emission)
		_well_ordered = false;
	    _last_emission = p->timestamp_anno();
	}
	input_s &is = _input[_pkt[0].input];
	++is.space;
	if (is.space == 1) {
	    _input[_nready].ready = _pkt[0].input;
	    ++_nready;
	}
	pop_heap(_pkt, _pkt + _npkt, heap_less());
	--_npkt;
	return p;
    } else {
	if (_stop && !signals_on)
	    router()->please_stop_driver();
	return 0;
    }
}

void
TimeSortedSched::add_handlers()
{
    add_data_handlers("well_ordered", Handler::f_read | Handler::f_checkbox, &_well_ordered);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimeSortedSched)

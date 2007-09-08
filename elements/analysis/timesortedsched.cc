// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * timesortedsched.{cc,hh} -- element merges sorted packet streams by timestamp
 * Eddie Kohler
 *
 * Copyright (c) 2001-2003 International Computer Science Institute
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
#include <click/confparse.hh>
#include <click/router.hh>
CLICK_DECLS

TimeSortedSched::TimeSortedSched()
    : _vec(0), _signals(0), _notifier(Notifier::SEARCH_CONTINUE_WAKE)
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
    _notifier.initialize(router());
    _stop = false;
    return cp_va_kparse(conf, this, errh,
			"STOP", 0, cpBool, &_stop,
		       cpEnd);
}

int 
TimeSortedSched::initialize(ErrorHandler *errh)
{
    _vec = new Packet*[ninputs()];
    _signals = new NotifierSignal[ninputs()];
    if (!_vec || !_signals)
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++) {
	_vec[i] = 0;
	_signals[i] = Notifier::upstream_empty_signal(this, i, 0, &_notifier);
    }
    return 0;
}

void
TimeSortedSched::cleanup(CleanupStage)
{
    if (_vec)
	for (int i = 0; i < ninputs(); i++)
	    if (_vec[i])
		_vec[i]->kill();
    delete[] _vec;
    delete[] _signals;
}

Packet*
TimeSortedSched::pull(int)
{
    int which = -1;
    Timestamp* tv = 0;
    bool signals_on = false;
    
    for (int i = 0; i < ninputs(); i++) {
	if (!_vec[i] && _signals[i]) {
	    _vec[i] = input(i).pull();
	    signals_on = true;
	}
	if (_vec[i]) {
	    Timestamp* this_tv = &_vec[i]->timestamp_anno();
	    if (!tv || *this_tv < *tv) {
		which = i;
		tv = this_tv;
	    }
	}
    }

    _notifier.set_active(which >= 0 || signals_on);
    if (which >= 0) {
	Packet *p = _vec[which];
	_vec[which] = input(which).pull();
	return p;
    } else {
	if (_stop && !signals_on)
	    router()->please_stop_driver();
	return 0;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimeSortedSched)

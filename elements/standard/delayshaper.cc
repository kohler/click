// -*- c-basic-offset: 4 -*-
/*
 * delayshaper.{cc,hh} -- element pulls packets from input, delays returnign
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/args.hh>
#include <click/glue.hh>
#include "delayshaper.hh"
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

DelayShaper::DelayShaper()
    : _p(0), _timer(this), _notifier(Notifier::SEARCH_CONTINUE_WAKE)
{
}

void *
DelayShaper::cast(const char *n)
{
    if (strcmp(n, "DelayShaper") == 0)
	return (DelayShaper *)this;
    else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return &_notifier;
    else
	return Element::cast(n);
}

int
DelayShaper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    return Args(conf, this, errh).read_mp("DELAY", _delay).complete();
}

int
DelayShaper::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _upstream_signal = Notifier::upstream_empty_signal(this, 0, &_notifier);
    return 0;
}

void
DelayShaper::cleanup(CleanupStage)
{
    if (_p)
	_p->kill();
}

Packet *
DelayShaper::pull(int)
{
    // read a packet
    if (!_p && (_p = input(0).pull())) {
	if (!_p->timestamp_anno().sec()) // get timestamp if not set
	    _p->timestamp_anno().assign_now();
	_p->timestamp_anno() += _delay;
    }

    if (_p) {
	Timestamp now = Timestamp::now();
	if (_p->timestamp_anno() <= now) {
	    // packet ready for output
	    Packet *p = _p;
	    p->timestamp_anno() = now;
	    _p = 0;
	    return p;
	}

	// adjust time by a bit
	Timestamp expiry = _p->timestamp_anno() - Timer::adjustment();
	if (expiry <= now) {
	    // small delta, don't go to sleep -- but mark our Signal as active,
	    // since we have something ready.
	    _notifier.wake();
	} else {
	    // large delta, go to sleep and schedule Timer
	    _timer.schedule_at(expiry);
	    _notifier.sleep();
	}
    } else if (!_upstream_signal) {
	// no packet available, we go to sleep right away
	_notifier.sleep();
    }

    return 0;
}

void
DelayShaper::run_timer(Timer *)
{
    _notifier.wake();
}

String
DelayShaper::read_param(Element *e, void *)
{
    DelayShaper *u = (DelayShaper *)e;
    return u->_delay.unparse_interval();
}

int
DelayShaper::write_param(const String &s, Element *e, void *, ErrorHandler *errh)
{
    DelayShaper *u = (DelayShaper *)e;
    if (!cp_time(s, &u->_delay))
	return errh->error("delay must be a timestamp");
    return 0;
}

void
DelayShaper::add_handlers()
{
    add_read_handler("delay", read_param, 0, Handler::h_calm);
    add_write_handler("delay", write_param, 0, Handler::h_nonexclusive);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DelayShaper)
ELEMENT_MT_SAFE(DelayShaper)

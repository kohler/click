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
#include <click/confparse.hh>
#include <click/glue.hh>
#include "delayshaper.hh"
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

DelayShaper::DelayShaper()
    : Element(1, 1), _p(0), _timer(this)
{
    MOD_INC_USE_COUNT;
}

DelayShaper::~DelayShaper()
{
    MOD_DEC_USE_COUNT;
}

void *
DelayShaper::cast(const char *n)
{
    if (strcmp(n, "DelayShaper") == 0)
	return (DelayShaper *)this;
    else if (strcmp(n, "Notifier") == 0)
	return static_cast<Notifier *>(this);
    else
	return Element::cast(n);
}

Notifier::SearchOp
DelayShaper::notifier_search_op()
{
    return SEARCH_UPSTREAM_LISTENERS;
}

int
DelayShaper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    ActiveNotifier::initialize(router());
    return cp_va_parse(conf, this, errh,
		       cpInterval, "delay", &_delay, 0);
}

int
DelayShaper::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _upstream_signal = Notifier::upstream_pull_signal(this, 0, 0);
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
	if (!_p->timestamp_anno().tv_sec) // get timestamp if not set
	    click_gettimeofday(&_p->timestamp_anno());
	_p->timestamp_anno() += _delay;
    }
    
    if (_p) {
	struct timeval now, diff;
	click_gettimeofday(&now);
	diff = _p->timestamp_anno() - now;
	
	if (diff.tv_sec < 0 || (diff.tv_sec == 0 && diff.tv_usec == 0)) {
	    // packet ready for output
	    Packet *p = _p;
	    p->timestamp_anno() = now;
	    _p = 0;
	    return p;
	} else if (diff.tv_sec == 0 && diff.tv_usec < 100000) {
	    // small delta, don't go to sleep -- but mark our Signal as active,
	    // since we have something ready. NB: should not wake listeners --
	    // we are in pull()!
	    set_signal_active(true);
	} else {
	    // large delta, go to sleep and schedule Timer
	    _timer.schedule_at(_p->timestamp_anno());
	    sleep_listeners();
	}
    } else if (!_upstream_signal) {
	// no packet available, we go to sleep right away
	sleep_listeners();
    }

    return 0;
}

void
DelayShaper::run_timer()
{
    wake_listeners();
}

String
DelayShaper::read_param(Element *e, void *)
{
    DelayShaper *u = (DelayShaper *)e;
    return cp_unparse_interval(u->_delay) + "\n";
}

void
DelayShaper::add_handlers()
{
    add_read_handler("delay", read_param, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DelayShaper)
ELEMENT_MT_SAFE(DelayShaper)

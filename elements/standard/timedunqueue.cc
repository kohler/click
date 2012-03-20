/*
 * timedunqueue.{cc,hh} -- element pulls packets from input, pushes to output
 * in bursts
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2009 Meraki, Inc.
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
#include "timedunqueue.hh"
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
CLICK_DECLS

TimedUnqueue::TimedUnqueue()
    : _burst(1), _task(this), _timer(&_task)
{
}

int
TimedUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh).read_mp("INTERVAL", SecondsArg(3), _interval)
	.read_p("BURST", _burst).complete() < 0)
	return -1;
    if (_burst <= 0)
	return errh->error("bad BURST");
    return 0;
}

int
TimedUnqueue::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, true, errh);
    _timer.initialize(this);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

bool
TimedUnqueue::run_task(Task *)
{
    // don't run if the timer is scheduled (an upstream queue went nonempty
    // but we don't care)
    if (_timer.scheduled())
	return false;

    for (int i = 0; i < _burst; i++) {
	Packet *p = input(0).pull();
	if (!p) {
	    if (use_signal && !_signal && i == 0) {
		_timer.clear();
		return false;	// without rescheduling
	    }
	    break;
	}
	output(0).push(p);
    }

    // reset timer
    if (_timer.expiry())
	_timer.reschedule_after_msec(_interval);
    else
	_timer.schedule_after_msec(_interval);
    return true;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimedUnqueue)

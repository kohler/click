/*
 * delayunqueue.{cc,hh} -- element pulls packets from input, delays pushing
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include "delayunqueue.hh"
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

DelayUnqueue::DelayUnqueue()
    : _p(0), _task(this), _timer(&_task)
{
}

int
DelayUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("DELAY", _delay).complete();
}

int
DelayUnqueue::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, errh);
    _timer.initialize(this);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

void
DelayUnqueue::cleanup(CleanupStage)
{
    if (_p)
	_p->kill();
}

bool
DelayUnqueue::run_task(Task *)
{
    bool worked = false;

  retry:
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
	    output(0).push(_p);
	    _p = 0;
	    worked = true;
	    goto retry;
	}

	Timestamp expiry = _p->timestamp_anno() - Timer::adjustment();
	if (expiry <= now)
	    // small delta, reschedule Task
	    /* Task rescheduled below */;
	else {
	    // large delta, schedule Timer
	    _timer.schedule_at(expiry);
	    return false;		// without rescheduling
	}
    } else {
	// no Packet available
	if (!_signal)
	    return false;		// without rescheduling
    }

    _task.fast_reschedule();
    return worked;
}

void
DelayUnqueue::add_handlers()
{
    add_data_handlers("delay", Handler::OP_READ | Handler::OP_WRITE, &_delay, true);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DelayUnqueue)

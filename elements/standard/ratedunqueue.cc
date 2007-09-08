// -*- c-basic-offset: 4 -*-
/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "ratedunqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

RatedUnqueue::RatedUnqueue()
    : _task(this)
{
}

RatedUnqueue::~RatedUnqueue()
{
}

int
RatedUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned r;
    CpVaParseCmd cmd = (is_bandwidth() ? cpBandwidth : cpUnsigned);
    if (cp_va_kparse(conf, this, errh, 
		     "RATE", cpkP+cpkM, cmd, &r, cpEnd) < 0) 
	return -1;
    _rate.set_rate(r, errh);
    return 0;
}

void
RatedUnqueue::configuration(Vector<String> &conf) const
{
    conf.push_back(is_bandwidth() ? cp_unparse_bandwidth(_rate.rate()) : String(_rate.rate()));
}

int
RatedUnqueue::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

bool
RatedUnqueue::run_task(Task *)
{
#if 1    // listening for notifications
    bool worked = false;
    if (_rate.need_update(Timestamp::now())) {
	//_rate.update();  // uncomment this if you want it to run periodically
	if (Packet *p = input(0).pull()) {
	    _rate.update();  
	    output(0).push(p);
	    worked = true;
	} else  // no Packet available
	    if (!_signal)
		return false;		// without rescheduling
    }
    _task.fast_reschedule();
    return worked;

#else   // no notification
    bool worked = false;
    if (_rate.need_update(Timestamp::now())) {
	if (Packet *p = input(0).pull()) {
	    _rate.update();
	    output(0).push(p);
	    worked = true;
	}
    }
    _task.fast_reschedule();
    return worked;
#endif
}


// HANDLERS

void
RatedUnqueue::add_handlers()
{
    add_read_handler("rate", read_positional_handler, 0);
    add_write_handler("rate", reconfigure_positional_handler, 0);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RatedUnqueue)
ELEMENT_MT_SAFE(RatedUnqueue)

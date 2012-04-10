// -*- c-basic-offset: 4 -*-
/*
 * handlertask.{cc,hh} -- task that calls a handler when scheduled
 * Eddie Kohler
 *
 * Copyright (c) 2010 Regents of the University of California
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
#include "handlertask.hh"
#include <click/args.hh>
#include <click/router.hh>
#include <click/handlercall.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

HandlerTask::HandlerTask()
    : _task(this), _count(0), _active(true), _reschedule(false)
{
}

int
HandlerTask::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("HANDLER", HandlerCallArg(HandlerCall::writable), _h)
	.read("ACTIVE", _active)
	.read("RESCHEDULE", _reschedule)
	.complete();
}

int
HandlerTask::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return _h.initialize_write(this, errh);
}

bool
HandlerTask::run_task(Task *)
{
    ++_count;
    _h.call_write(ErrorHandler::default_handler());
    if (_reschedule)
	_task.fast_reschedule();
    return true;
}

void
HandlerTask::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("reschedule", Handler::OP_READ | Handler::OP_WRITE, &_reschedule);
    add_task_handlers(&_task, 0, TASKHANDLER_WRITE_ALL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HandlerTask)

// -*- c-basic-offset: 4 -*-
/*
 * nulltask.{cc,hh} -- schedule a do-nothing task
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
#include "nulltask.hh"
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

NullTask::NullTask()
    : _count(0), _limit(0), _task(this), _stop(false), _active(true)
{
}

int
NullTask::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read("STOP", _stop)
	.read("LIMIT", _limit)
	.read("ACTIVE", _active)
	.complete();
}

int
NullTask::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

bool
NullTask::run_task(Task *)
{
    ++_count;
    if (_limit == 0 || _count < _limit)
	_task.fast_reschedule();
    if (_limit != 0 && _count >= _limit && _stop)
	router()->please_stop_driver();
    return true;
}

int
NullTask::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    NullTask *nt = static_cast<NullTask *>(e);
    nt->_count = 0;
    return 0;
}

void
NullTask::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("limit", Handler::OP_READ | Handler::OP_WRITE, &_limit);
    add_write_handler("reset", write_handler);
    add_task_handlers(&_task, 0, TASKHANDLER_WRITE_ALL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NullTask)

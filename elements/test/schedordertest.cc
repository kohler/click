// -*- c-basic-offset: 4 -*-
/*
 * schedordertest.{cc,hh} -- remember scheduling order
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "schedordertest.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

SchedOrderTest::SchedOrderTest()
    : _count(0), _limit(0), _buf_begin(0), _bufsiz(0),
      _task(this), _stop(false)
{
}

SchedOrderTest::~SchedOrderTest()
{
    if (_bufpos_ptr == &_bufpos)
	delete[] _buf_begin;
}

int
SchedOrderTest::configure(Vector<String>& conf, ErrorHandler* errh)
{
    if (Args(conf, this, errh)
	.read_mp("ID", _id)
	.read("SIZE", _bufsiz)
	.read("STOP", _stop)
	.read("LIMIT", _limit)
	.complete() < 0)
	return -1;

    void*& main = router()->force_attachment("SchedOrderTest");
    if (!main)
	main = this;

    SchedOrderTest* sot = (SchedOrderTest*) main;
    if (sot->_bufsiz < _bufsiz)
	sot->_bufsiz = _bufsiz;

    return 0;
}

int
SchedOrderTest::initialize(ErrorHandler* errh)
{
    SchedOrderTest* sot = (SchedOrderTest*) router()->attachment("SchedOrderTest");
    if (!sot->_buf_begin) {
	if (sot->_bufsiz == 0)
	    sot->_bufsiz = 1024;
	if (!(sot->_buf_begin = new int[sot->_bufsiz]))
	    return errh->error("out of memory!");
	sot->_buf_end = sot->_buf_begin + sot->_bufsiz;
	sot->_bufpos = sot->_buf_begin;
    }

    _buf_begin = sot->_buf_begin;
    _bufpos_ptr = &sot->_bufpos;
    _buf_end = sot->_buf_end;
    ScheduleInfo::initialize_task(this, &_task, true, errh);
    return 0;
}

bool
SchedOrderTest::run_task(Task *)
{
    if (*_bufpos_ptr < _buf_end) {
	*((*_bufpos_ptr)++) = _id;
    } else if (_stop)
	router()->please_stop_driver();
    if (_limit == 0 || ++_count < _limit)
	_task.fast_reschedule();
    return true;
}

String
SchedOrderTest::read_handler(Element* e, void*)
{
    SchedOrderTest* sot = (SchedOrderTest*) e;
    StringAccum sa;
    for (int* x = sot->_buf_begin; x < *sot->_bufpos_ptr; x++)
	sa << *x << ' ';
    sa.back() = '\n';
    return sa.take_string();
}

void
SchedOrderTest::add_handlers()
{
    add_read_handler("order", read_handler);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SchedOrderTest)

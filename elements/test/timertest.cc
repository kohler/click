// -*- c-basic-offset: 4 -*-
/*
 * timertest.{cc,hh} -- regression test element for Timer
 * Eddie Kohler
 *
 * Copyright (c) 2009 Intel Corporation
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
#include "timertest.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/master.hh>
CLICK_DECLS

TimerTest::TimerTest()
    : _timer(this), _benchmark(0)
{
}

TimerTest::~TimerTest()
{
}

int
TimerTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Timestamp delay;
    bool schedule = false;
    if (Args(conf, this, errh)
	.read("BENCHMARK", _benchmark)
	.read("DELAY", delay)
	.read("SCHEDULE", schedule)
	.complete() < 0)
	return -1;
    _timer.initialize(this);
    if (schedule || delay)
	_timer.schedule_after(delay);
    return 0;
}

int
TimerTest::initialize(ErrorHandler *)
{
    if (_timer.scheduled())
	/* do nothing */;
    else if (_benchmark <= 0) {
	Timer default_constructor_timer;
	Timer explicit_do_nothing_timer = Timer::do_nothing_t();

	click_chatter("Initializing default_constructor_timer");
	default_constructor_timer.initialize(this);
	click_chatter("Initializing explicit_do_nothing_timer");
	explicit_do_nothing_timer.initialize(this);
    } else {
	Timestamp now = Timestamp::now_steady();
	Timer *ts = new Timer[_benchmark];
	for (int i = 0; i < _benchmark; ++i) {
	    ts[i].assign();
	    ts[i].initialize(this);
	}
	benchmark_schedules(ts, _benchmark, now);
	benchmark_changes(ts, _benchmark, now);
	benchmark_fires(ts, _benchmark, now);
	delete[] ts;
    }

    return 0;
}

void
TimerTest::run_timer(Timer *t)
{
    click_chatter("%p{timestamp}: %p{element} fired", &t->expiry_steady(), this);
}

void
TimerTest::benchmark_schedules(Timer *ts, int nts, const Timestamp &now)
{
    for (int i = 0; i < nts; ++i)
	ts[i].schedule_at_steady(now + Timestamp::make_msec(click_random(0, 10000)));
}

void
TimerTest::benchmark_changes(Timer *ts, int nts, const Timestamp &now)
{
    RouterThread *th = ts->thread();
    for (int i = 0; i < 6 * nts; ++i) {
	Timer *t;
	if (click_random(0, 8) < 6) {
	    t = th->timer_set().next_timer();
	    t->unschedule();
	} else
	    t = &ts[click_random(0, nts - 1)];
	t->schedule_at_steady(now + Timestamp::make_msec(click_random(0, 10000)));
    }
}

void
TimerTest::benchmark_fires(Timer *ts, int, const Timestamp &)
{
    RouterThread *th = ts->thread();
    while (Timer *t = th->timer_set().next_timer())
	t->unschedule();
}

String
TimerTest::read_handler(Element *e, void *user_data)
{
    TimerTest *tt = static_cast<TimerTest *>(e);
    switch ((uintptr_t) user_data) {
    case h_scheduled:
	return String(tt->_timer.scheduled());
    case h_expiry:
    default:
	return String(tt->_timer.expiry_steady());
    }
}

int
TimerTest::write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    TimerTest *tt = static_cast<TimerTest *>(e);
    switch ((uintptr_t) user_data) {
    case h_scheduled: {
	bool schedule;
	if (!BoolArg().parse(str, schedule))
	    return errh->error("syntax error");
	if (schedule)
	    tt->_timer.schedule_at_steady(tt->_timer.expiry_steady());
	else
	    tt->_timer.unschedule();
	break;
    }
    case h_schedule_after: {
	Timestamp delay;
	if (!TimestampArg().parse(str, delay))
	    return errh->error("syntax error");
	tt->_timer.schedule_after(delay);
	break;
    }
    case h_unschedule:
	tt->_timer.unschedule();
	break;
    }
    return 0;
}

void
TimerTest::add_handlers()
{
    add_read_handler("scheduled", read_handler, h_scheduled);
    add_write_handler("scheduled", write_handler, h_scheduled);
    add_read_handler("expiry", read_handler, h_expiry);
    add_write_handler("schedule_after", write_handler, h_schedule_after);
    add_write_handler("unschedule", write_handler, h_unschedule);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimerTest)

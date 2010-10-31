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
#include <click/confparse.hh>
#include <click/master.hh>
CLICK_DECLS

TimerTest::TimerTest()
{
}

TimerTest::~TimerTest()
{
}

int
TimerTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_kparse(conf, this, errh,
			"BENCHMARK", 0, cpInteger, &_benchmark,
			cpEnd);
}

int
TimerTest::initialize(ErrorHandler *)
{
    if (_benchmark <= 0) {
	Timer default_constructor_timer;
	Timer explicit_do_nothing_timer = Timer::do_nothing_t();

	click_chatter("Initializing default_constructor_timer");
	default_constructor_timer.initialize(this);
	click_chatter("Initializing explicit_do_nothing_timer");
	explicit_do_nothing_timer.initialize(this);
    } else {
	Timestamp now = Timestamp::now();
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
TimerTest::benchmark_schedules(Timer *ts, int nts, const Timestamp &now)
{
    for (int i = 0; i < nts; ++i)
	ts[i].schedule_at(now + Timestamp::make_msec(click_random(0, 10000)));
}

void
TimerTest::benchmark_changes(Timer *ts, int nts, const Timestamp &now)
{
    Master *m = master();
    for (int i = 0; i < 6 * nts; ++i) {
	Timer *t;
	if (click_random(0, 8) < 6) {
	    t = m->next_timer();
	    t->unschedule();
	} else
	    t = &ts[click_random(0, nts - 1)];
	t->schedule_at(now + Timestamp::make_msec(click_random(0, 10000)));
    }
}

void
TimerTest::benchmark_fires(Timer *, int, const Timestamp &)
{
    Master *m = master();
    while (Timer *t = m->next_timer())
	t->unschedule();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimerTest)

// -*- c-basic-offset: 4 -*-
/*
 * threadmonitor.{cc,hh} -- bin-packing sort for tasks (SMP Click)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "threadmonitor.hh"
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/error.hh>

ThreadMonitor::ThreadMonitor()
    : _timer(this)
{
}

ThreadMonitor::~ThreadMonitor()
{
}

int
ThreadMonitor::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _interval = 1000;
    _thresh = 1000;
    if (Args(conf, this, errh)
	.read_p("INTERVAL", _interval)
	.read_p("THRESH", _thresh)
	.complete() < 0)
	return -1;
    return 0;
}

int
ThreadMonitor::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_msec(10);
    return 0;
}

void
ThreadMonitor::run_timer(Timer *)
{
    Master *m = router()->master();
    StringAccum sa;
    click_jiffies_t now_jiffies = click_jiffies();
    Vector<Task *> tasks;

    // report currently scheduled tasks (ignore pending list)
    for (int tid = 0; tid < m->nthreads(); tid++) {
	tasks.clear();
	m->thread(tid)->scheduled_tasks(router(), tasks);
	for (Task **t = tasks.begin(); t != tasks.end(); ++t)
	    if ((*t)->cycles() >= _thresh) {
		sa << now_jiffies << ": on thread " << tid << ": "
		   << (void *)*t
		   << " (" << (*t)->element()->declaration()
		   << "), cycles " << (*t)->cycles() << '\n';
	    }
    }

    if (sa.length())
	click_chatter("%s", sa.c_str());

    _timer.schedule_after_msec(_interval);
}

ELEMENT_REQUIRES(linuxmodule smpclick)
EXPORT_ELEMENT(ThreadMonitor)

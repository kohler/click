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
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/error.hh>

ThreadMonitor::ThreadMonitor()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

ThreadMonitor::~ThreadMonitor()
{
    MOD_DEC_USE_COUNT;
}

int 
ThreadMonitor::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _interval = 1000;
    _thresh = 1000;
    if (cp_va_parse(conf, this, errh, 
		    cpOptional,
		    cpUnsigned, "interval", &_interval,
		    cpUnsigned, "threshold", &_thresh, 0) < 0)
	return -1;
    return 0;
}

int
ThreadMonitor::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_ms(10);
    return 0;
}

void
ThreadMonitor::run_timer()
{
    Master *m = router()->master();
    StringAccum sa;
    unsigned now_jiffies = click_jiffies();

    // report currently scheduled tasks (ignore pending list)
    for (int threadid = 0; threadid < m->nthreads(); threadid++) {
	RouterThread *thread = m->thread(threadid);
	thread->lock_tasks();
	for (Task *t = thread->scheduled_next(); t != thread; t = t->scheduled_next())
	    if (t->cycles() >= _thresh) {
		sa << now_jiffies << ": on " << threadid << ": " << (void *)t << " (";
		if (Element *e = t->element())
		    sa << e->id();
		else
		    sa << "hook";
		sa << "), cycles " << t->cycles();
	    }
	thread->unlock_tasks();
    }

    if (sa.length())
	click_chatter("%s", sa.c_str());

    _timer.schedule_after_ms(_interval);
}

ELEMENT_REQUIRES(linuxmodule smpclick)
EXPORT_ELEMENT(ThreadMonitor)

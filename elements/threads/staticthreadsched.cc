/*
 * staticthreadsched.{cc,hh} -- element statically assigns tasks to threads
 * Eddie Kohler
 *
 * Copyright (c) 2004-2008 Regents of the University of California
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
#include "staticthreadsched.hh"
#include <click/task.hh>
#include <click/master.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/args.hh>
CLICK_DECLS

StaticThreadSched::StaticThreadSched()
    : _next_thread_sched(0)
{
}

StaticThreadSched::~StaticThreadSched()
{
}

int
StaticThreadSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e;
    int preference;
    for (int i = 0; i < conf.size(); i++) {
	if (Args(this, errh).push_back_words(conf[i])
	    .read_mp("ELEMENT", e)
	    .read_mp("THREAD", preference)
	    .complete() < 0)
	    return -1;
	if (e->eindex() >= _thread_preferences.size())
	    _thread_preferences.resize(e->eindex() + 1, THREAD_UNKNOWN);
	if (preference < -1 || preference >= master()->nthreads()) {
	    errh->warning("thread preference %d out of range", preference);
	    preference = (preference < 0 ? -1 : 0);
	}
	_thread_preferences[e->eindex()] = preference;
    }
    _next_thread_sched = router()->thread_sched();
    router()->set_thread_sched(this);
    return 0;
}

int
StaticThreadSched::initial_home_thread_id(Element *owner, Task *task,
					  bool scheduled)
{
    int eidx = owner->eindex();
    if (eidx >= 0 && eidx < _thread_preferences.size()
	&& _thread_preferences[eidx] != THREAD_UNKNOWN)
	return _thread_preferences[eidx];
    if (_next_thread_sched)
	return _next_thread_sched->initial_home_thread_id(owner, task, scheduled);
    else
	return THREAD_UNKNOWN;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StaticThreadSched)

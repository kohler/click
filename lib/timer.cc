// -*- c-basic-offset: 4; related-file-name: "../include/click/timer.hh" -*-
/*
 * timer.{cc,hh} -- portable timers
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
#include <click/timer.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/routerthread.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * element_hook is a callback that gets called when a Timer,
 * constructed with just an Element instance, expires. 
 * 
 * When used in userlevel or kernel polling mode, timer is maintained by
 * Click, so element_hook is called within Click.
 */

static void
element_hook(Timer*, void* thunk)
{
    Element* e = (Element*)thunk;
    e->run_timer();
}

static void
task_hook(Timer*, void* thunk)
{
    Task* task = (Task*)thunk;
    task->reschedule();
}


Timer::Timer(TimerHook hook, void* thunk)
    : _schedpos(-1), _hook(hook), _thunk(thunk), _router(0)
{
}

Timer::Timer(Element* e)
    : _schedpos(-1), _hook(element_hook), _thunk(e), _router(0)
{
}

Timer::Timer(Task* t)
    : _schedpos(-1), _hook(task_hook), _thunk(t), _router(0)
{
}

void
Timer::schedule_at(const Timestamp& when)
{
    // acquire lock, unschedule
    assert(_router && initialized());
    Master* master = _router->master();
    master->_timer_lock.acquire();

    // set expiration timer
    _expiry = when;

    // manipulate list; this is essentially a "decrease-key" operation
    if (!scheduled()) {
	_schedpos = master->_timer_heap.size();
	master->_timer_heap.push_back(0);
    }
    master->timer_reheapify_from(_schedpos, this);

    // if we changed the timeout, wake up the first thread
    if (_schedpos == 0)
	master->_threads[1]->unsleep();

    // done
    master->_timer_lock.release();
}

void
Timer::schedule_after(const Timestamp &delta)
{
    schedule_at(Timestamp::now() + delta);
}

void
Timer::unschedule()
{
    if (scheduled()) {
	Master* master = _router->master();
	master->_timer_lock.acquire();
	master->timer_reheapify_from(_schedpos, master->_timer_heap.back());
	_schedpos = -1;
	master->_timer_heap.pop_back();
	master->_timer_lock.release();
    }
}

// list-related functions in master.cc

CLICK_ENDDECLS

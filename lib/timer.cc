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


/** @class Timer
 * @brief Represents a computational task that should execute at a given time.
 *
 * 
 *
 */

/*
 * element_hook is a callback that gets called when a Timer,
 * constructed with just an Element instance, expires. 
 * 
 * When used in userlevel or kernel polling mode, timer is maintained by
 * Click, so element_hook is called within Click.
 */
static void
element_hook(Timer *timer, void *thunk)
{
    Element* e = (Element*)thunk;
    e->run_timer(timer);
}

static void
task_hook(Timer*, void* thunk)
{
    Task* task = (Task*)thunk;
    task->reschedule();
}


/** @brief Create a Timer that will call @a hook(this, @a thunk) when fired.
 *
 * @param hook the callback function
 * @param thunk argument for the callback function
 */
Timer::Timer(TimerHook hook, void* thunk)
    : _schedpos(-1), _hook(hook), _thunk(thunk), _router(0)
{
}

/** @brief Create a Timer that will call @a element ->@link
 * Element::run_timer() run_timer@endlink(this) when fired.
 *
 * @param element the element
 */
Timer::Timer(Element* element)
    : _schedpos(-1), _hook(element_hook), _thunk(element), _router(0)
{
}

/** @brief Create a Timer that will schedule @a task when fired.
 *
 * @param task the task
 */
Timer::Timer(Task* task)
    : _schedpos(-1), _hook(task_hook), _thunk(task), _router(0)
{
}

/** @brief Schedule the timer to fire at @a when.
 *
 * @param when expiration time
 */
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
	master->_threads[2]->wake();

    // done
    master->_timer_lock.release();
}

/** @brief Schedule the timer to fire @a delta time in the future.
 *
 * @param delta interval until expiration time
 *
 * The schedule_after methods schedule the timer relative to the current time,
 * Timestamp::now().  When called from a timer's expiration hook, this will
 * usually be slightly after the timer's nominal expiration time.  To schedule
 * a timer at a strict interval, compensating for any drift, use the
 * reschedule_after methods.
 */
void
Timer::schedule_after(const Timestamp &delta)
{
    schedule_at(Timestamp::now() + delta);
}

/** @brief Unschedule the timer.
 *
 * The timer's expiration time is not modified.
 */
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

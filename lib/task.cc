// -*- c-basic-offset: 4; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include <click/task.hh>
#include <click/router.hh>
#include <click/routerthread.hh>
#include <click/master.hh>
CLICK_DECLS

// - Changes to _thread are protected by _thread->lock.
// - Changes to _thread_preference are protected by
//   _router->master()->task_lock.
// - If _pending is nonzero, then _pending_next is nonnull.
// - Either _thread_preference == _thread->thread_id(), or
//   _thread->thread_id() == -1.

bool
Task::error_hook(Task *, void *)
{
    assert(0);
    return false;
}

void
Task::make_list()
{
    _hook = error_hook;
    _pending_next = this;
}

Task::~Task()
{
    assert(!scheduled() || _thread == this);
}

Master *
Task::master() const
{
    assert(_thread);
    return _thread->master();
}

void
Task::initialize(Router *router, bool join)
{
    assert(!initialized() && !scheduled());

    _router = router;
    
    _thread_preference = router->initial_thread_preference(this, join);
    if (_thread_preference == ThreadSched::THREAD_PREFERENCE_UNKNOWN)
	_thread_preference = 0;
    // Master::thread() returns the quiescent thread if its argument is out of
    // range
    _thread = router->master()->thread(_thread_preference);
    
#ifdef HAVE_STRIDE_SCHED
    set_tickets(DEFAULT_TICKETS);
#endif

    if (join)
	add_pending(RESCHEDULE);
}

void
Task::initialize(Element *e, bool join)
{
    initialize(e->router(), join);
}

void
Task::cleanup()
{
    if (initialized()) {
	strong_unschedule();

	if (_pending) {
	    Master *m = _router->master();
	    SpinlockIRQ::flags_t flags = m->_task_lock.acquire();
	    Task *prev = &m->_task_list;
	    for (Task *t = prev->_pending_next; t != &m->_task_list; prev = t, t = t->_pending_next)
		if (t == this) {
		    prev->_pending_next = t->_pending_next;
		    break;
		}
	    _pending = 0;
	    _pending_next = 0;
	    m->_task_lock.release(flags);
	}
	
	_router = 0;
	_thread = 0;
    }
}

inline void
Task::lock_tasks()
{
    while (1) {
	RouterThread *t = _thread;
	t->lock_tasks();
	if (t == _thread)
	    return;
	t->unlock_tasks();
    }
}

inline bool
Task::attempt_lock_tasks()
{
    RouterThread *t = _thread;
    if (t->attempt_lock_tasks()) {
	if (t == _thread)
	    return true;
	t->unlock_tasks();
    }
    return false;
}

void
Task::add_pending(int p)
{
    Master *m = _router->master();
    SpinlockIRQ::flags_t flags = m->_task_lock.acquire();
    if (_router->_running >= Router::RUNNING_PAUSED) {
	_pending |= p;
	if (!_pending_next && _pending) {
	    _pending_next = m->_task_list._pending_next;
	    m->_task_list._pending_next = this;
	}
	if (_pending)
	    _thread->add_pending();
    }
    m->_task_lock.release(flags);
}

void
Task::unschedule()
{
    // Thanksgiving 2001: unschedule() will always unschedule the task. This
    // seems more reliable, since some people depend on unschedule() ensuring
    // that the task is not scheduled any more, no way, no how. Possible
    // problem: calling unschedule() from run_task() will hang!
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
    if (_thread) {
	lock_tasks();
	fast_unschedule();
	_pending &= ~RESCHEDULE;
	_thread->unlock_tasks();
    }
}

void
Task::true_reschedule()
{
    assert(_thread);
    bool done = false;
#if CLICK_LINUXMODULE
    if (in_interrupt())
	/* nada */;
    else
#endif
    if (attempt_lock_tasks()) {
	if (_router->_running >= Router::RUNNING_BACKGROUND) {
	    if (!scheduled()) {
		fast_schedule();
		_thread->unsleep();
	    }
	    done = true;
	}
	_thread->unlock_tasks();
    }
    if (!done)
	add_pending(RESCHEDULE);
}

void
Task::strong_unschedule()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
    // unschedule() and move to the quiescent thread, so that subsequent
    // reschedule()s won't have any effect
    if (_thread) {
	lock_tasks();
	fast_unschedule();
	RouterThread *old_thread = _thread;
	_pending &= ~(RESCHEDULE | CHANGE_THREAD);
	_thread = _router->master()->thread(-1);
	old_thread->unlock_tasks();
    }
}

void
Task::strong_reschedule()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
    assert(_thread);
    lock_tasks();
    fast_unschedule();
    RouterThread *old_thread = _thread;
    _thread = _router->master()->thread(_thread_preference);
    add_pending(RESCHEDULE);
    old_thread->unlock_tasks();
}

void
Task::change_thread(int new_preference)
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
    _thread_preference = new_preference;
    // no need to verify _thread_preference; Master::thread() returns the
    // quiescent thread if its argument is out of range

    if (attempt_lock_tasks()) {
	RouterThread *old_thread = _thread;
	if (_thread_preference != old_thread->thread_id()) {
	    if (scheduled()) {
		fast_unschedule();
		_pending |= RESCHEDULE;
	    }
	    _thread = _router->master()->thread(_thread_preference);
	    old_thread->unlock_tasks();
	    add_pending(0);
	} else
	    old_thread->unlock_tasks();
    } else
	add_pending(CHANGE_THREAD);
}

void
Task::process_pending(RouterThread *thread)
{
    // must be called with thread->lock held

    if (_thread == thread) {
	if (_pending & CHANGE_THREAD) {
	    // see also change_thread() above
	    _pending &= ~CHANGE_THREAD;
	    if (scheduled()) {
		fast_unschedule();
		_pending |= RESCHEDULE;
	    }
	    _thread = _router->master()->thread(_thread_preference);
	} else if (_pending & RESCHEDULE) {
	    _pending &= ~RESCHEDULE;
	    if (!scheduled())
		fast_schedule();
	}
    }

    if (_pending)
	add_pending(0);
}

CLICK_ENDDECLS

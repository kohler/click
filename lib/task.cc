// -*- c-basic-offset: 4; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
 * Copyright (c) 1999-2012 Eddie Kohler
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

/** @file task.hh
 * @brief Support for Task, which triggers execution frequently.
 */

/** @class Task
 * @brief Represents a frequently-scheduled computational task.
 *
 * Click schedules a router's CPU or CPUs with one or more <em>task
 * queues</em>.  These queues are simply lists of @e tasks, which represent
 * functions that would like unconditional access to the CPU.  Tasks are
 * generally associated with elements.  When scheduled, most tasks call some
 * element's @link Element::run_task(Task *) run_task()@endlink method.
 *
 * Click tasks are represented by Task objects.  An element that would like
 * special access to a router's CPU should include and initialize a Task
 * instance variable.
 *
 * Tasks are called very frequently, up to tens of thousands of times per
 * second.  Elements generally use Tasks for frequent tasks, and implement
 * their own algorithms for scheduling and unscheduling the tasks when there's
 * work to be done.  For infrequent events, it is far more efficient to use
 * Timer objects.
 *
 * A Task's callback function, which is called when the task fires, has bool
 * return type.  The callback should return true if the task did useful work,
 * and false if it was not able to do useful work (for example, because there
 * were no packets in the configuration to return).  Adaptive algorithms may
 * use this information to fine-tune Click's scheduling behavior.
 *
 * Since Click tasks are cooperatively scheduled, executing a task should not
 * take a long time.  Slow tasks can inappropriately delay timers and other
 * periodic events.
 *
 * This code shows how Task objects tend to be used.  For fuller examples, see
 * InfiniteSource and similar elements.
 *
 * @code
 * class InfiniteSource { ...
 *     bool run_task(Task *t);
 *   private:
 *     Task _task;
 * };
 *
 * InfiniteSource::InfiniteSource() : _task(this) {
 * }
 *
 * int InfiniteSource::initialize(ErrorHandler *errh) {
 *     ScheduleInfo::initialize_task(this, &_task, errh);
 *     return 0;
 * }
 *
 * bool InfiniteSource::run_task(Task *) {
 *     Packet *p = ... generate packet ...;
 *     output(0).push(p);
 *     if (packets left to send)
 *         _task.fast_reschedule();
 *     return true;  // the task did useful work
 * }
 * @endcode
 */

// Invariants:
// - _home_thread_id, _is_scheduled, _is_strong_unscheduled may be changed
//   at any time without locking.
// - If _is_scheduled && !_is_strong_unscheduled && not on quiescent thread,
//   then either on_scheduled_list() or _pending_nextptr != 0.
// - _thread may be read at any time, but since it might change underneath,
//   read it into a local variable.
// - Changes to _thread are protected by _thread->_pending_lock.
//   Furthermore, only _thread itself may change _thread
//   (except that if _thread is quiescent, anyone may change _thread).
//   To arrange for _thread to change, set _home_thread_id and add_pending().
// - _pending_nextptr is protected by _thread->_pending_lock.
//   But after acquiring this lock, verify that _thread has not changed.

bool
Task::error_hook(Task *, void *)
{
    assert(0);
    return false;
}

Task::~Task()
{
    if (scheduled() || on_pending_list())
	cleanup();
}

Master *
Task::master() const
{
    assert(_thread);
    return _thread->master();
}


inline void
Task::add_pending_locked(RouterThread *thread)
{
    if (!_pending_nextptr.x) {
	_pending_nextptr.x = 1;
	thread->_pending_tail->t = this;
	thread->_pending_tail = &_pending_nextptr;
	thread->wake();
    }
}

void
Task::add_pending()
{
    bool thread_match;
    do {
	RouterThread *thread = _thread;
	SpinlockIRQ::flags_t flags = thread->_pending_lock.acquire();
	thread_match = thread == _thread;
	if (thread_match && thread->thread_id() >= 0)
	    add_pending_locked(thread);
	thread->_pending_lock.release(flags);
    } while (!thread_match);
}

inline void
Task::remove_pending_locked(RouterThread *thread)
{
    if (_pending_nextptr.x) {
	Pending *tptr = &thread->_pending_head;
	while (tptr->x > 1 && tptr->t != this)
	    tptr = &tptr->t->_pending_nextptr;
	if (tptr->t == this) {
	    *tptr = _pending_nextptr;
	    if (_pending_nextptr.x <= 1) {
		thread->_pending_tail = tptr;
		if (tptr == &thread->_pending_head)
		    tptr->x = 0;
	    }
	    _pending_nextptr.x = 0;
	}
    }
}

void
Task::remove_pending()
{
    bool thread_match;
    do {
	RouterThread *thread = _thread;
	SpinlockIRQ::flags_t flags = thread->_pending_lock.acquire();
	thread_match = thread == _thread;
	if (thread_match)
	    remove_pending_locked(thread);
	thread->_pending_lock.release(flags);
    } while (!thread_match);
}


void
Task::initialize(Element *owner, bool schedule)
{
    assert(owner && !initialized() && !scheduled());

    Router *router = owner->router();
    int tid = router->home_thread_id(owner);
    // Master::thread() returns the quiescent thread if its argument is out of
    // range
    _thread = router->master()->thread(tid);

    // set _owner last, since it is used to determine whether task is
    // initialized
    _owner = owner;

#if HAVE_STRIDE_SCHED
    set_tickets(DEFAULT_TICKETS);
#endif

    _status.home_thread_id = _thread->thread_id();
    _status.is_scheduled = schedule;
    if (schedule)
	add_pending();
}

void
Task::initialize(Router *router, bool schedule)
{
    initialize(router->root_element(), schedule);
}

void
Task::cleanup()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    if (initialized()) {
	strong_unschedule();
	remove_from_scheduled_list();

	// Perhaps the task is enqueued on the current pending
	// collection.  If so, remove it.
	if (on_pending_list()) {
	    remove_pending();

	    // If not on the current pending list, perhaps this task
	    // is on some list currently being processed by
	    // RouterThread::process_pending().  Wait until that
	    // processing is done.  It is safe to simply spin because
	    // pending list processing is so simple: processing a
	    // pending list will NEVER cause a task to get deleted, so
	    // ~Task is never called from RouterThread::process_pending().
	    while (on_pending_list())
		click_relax_fence();
	}

	_owner = 0;
	_thread = 0;
    }
}


void
Task::true_reschedule()
{
    bool done = false;
    RouterThread *thread = _thread;
    if (unlikely(thread == 0 || thread->thread_id() < 0))
	done = true;
    else if (thread->current_thread_is_running()) {
	Router *router = _owner->router();
	if (router->_running >= Router::RUNNING_BACKGROUND) {
	    fast_schedule();
	    done = true;
	}
    }
    if (!done)
	add_pending();
}

void
Task::move_thread_second_half()
{
    RouterThread *old_thread;
    SpinlockIRQ::flags_t flags;
    while (1) {
	old_thread = _thread;
	flags = old_thread->_pending_lock.acquire();
	if (old_thread == _thread)
	    break;
	old_thread->_pending_lock.release(flags);
    }

    if (old_thread->current_thread_is_running()
	|| old_thread->thread_id() < 0) {
	remove_from_scheduled_list();
	remove_pending_locked(old_thread);
	_thread = master()->thread(_status.home_thread_id);
	old_thread->_pending_lock.release(flags);

	if (_status.is_scheduled)
	    add_pending();
    } else {
	add_pending_locked(old_thread);
	old_thread->_pending_lock.release(flags);
    }
}

void
Task::move_thread(int new_thread_id)
{
    if (likely(_thread != 0)) {
	RouterThread *new_thread = master()->thread(new_thread_id);
	// (new_thread->thread_id() might != new_thread_id)
	_status.home_thread_id = new_thread->thread_id();
	if (_status.home_thread_id != _thread->thread_id())
	    move_thread_second_half();
    }
}

void
Task::process_pending(RouterThread *thread)
{
    // Must be called with thread->lock held.
    // May be called in the process of destroying router(), so must check
    // router()->_running when necessary.  (Not necessary for add_pending()
    // since that function does it already.)

    Task::Status status(_status);
    if (status.is_strong_unscheduled == 2) {
	// clean up is_strong_unscheduled values used for driver stop events
	Task::Status new_status(status);
	new_status.is_strong_unscheduled = false;
	atomic_uint32_t::compare_swap(_status.status, status.status, new_status.status);
    }

    if (status.home_thread_id != thread->thread_id())
	move_thread_second_half();
    else if (status.is_scheduled) {
	if (router()->running())
	    fast_schedule();
	else
	    add_pending();
    }
}

CLICK_ENDDECLS

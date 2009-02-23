// -*- c-basic-offset: 4; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
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

// - Changes to _thread are protected by _thread->lock.
// - Resetting _should_be_scheduled to 0 is protected by _thread->lock.
// - Changes to _home_thread_id are protected by
//   router()->master()->task_lock.

bool
Task::error_hook(Task *, void *)
{
    assert(0);
    return false;
}

Task::~Task()
{
#if HAVE_TASK_HEAP
    if (scheduled() || _pending_nextptr)
	cleanup();
#else
    if ((scheduled() || _pending_nextptr) && _thread != this)
	cleanup();
#endif
}

Master *
Task::master() const
{
    assert(_thread);
    return _thread->master();
}

void
Task::initialize(Element *owner, bool schedule)
{
    assert(owner && !initialized() && !scheduled());

    Router *router = owner->router();
    _home_thread_id = router->initial_home_thread_id(this, schedule);
    if (_home_thread_id == ThreadSched::THREAD_UNKNOWN)
	_home_thread_id = 0;
    // Master::thread() returns the quiescent thread if its argument is out of
    // range
    _thread = router->master()->thread(_home_thread_id);

    // set _owner last, since it is used to determine whether task is
    // initialized
    _owner = owner;

#if HAVE_STRIDE_SCHED
    set_tickets(DEFAULT_TICKETS);
#endif

    _should_be_scheduled = schedule;
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

	if (_pending_nextptr) {
	    // Perhaps the task is enqueued on the current pending collection.
	    // If so, remove it.
	    Master *m = _owner->master();
	    SpinlockIRQ::flags_t flags = m->_master_task_lock.acquire();
	    if (_pending_nextptr) {
		volatile uintptr_t *tptr = &m->_pending_head;
		while (Task *t = pending_to_task(*tptr))
		    if (t == this) {
			*tptr = _pending_nextptr;
			if (!pending_to_task(_pending_nextptr))
			    m->_pending_tail = tptr;
			_pending_nextptr = 0;
			break;
		    } else
			tptr = &t->_pending_nextptr;
	    }
	    m->_master_task_lock.release(flags);

	    // If not on the current pending list, perhaps this task is on
	    // some list currently being processed by
	    // Master::process_pending().  Wait until that processing is done.
	    // It is safe to simply spin because pending list processing is so
	    // simple: processing a pending list will NEVER cause a task to
	    // get deleted, so ~Task is never called from
	    // Master::process_pending().
	    while (_pending_nextptr)
		/* do nothing */;
	}

	_owner = 0;
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
Task::add_pending()
{
    Router *router = _owner->router();
    Master *m = router->master();
    SpinlockIRQ::flags_t flags = m->_master_task_lock.acquire();
    if (router->_running >= Router::RUNNING_PREPARING
	&& !_pending_nextptr) {
	*m->_pending_tail = reinterpret_cast<uintptr_t>(this);
	m->_pending_tail = &_pending_nextptr;
	_pending_nextptr = 1;
	_thread->add_pending();
    }
    m->_master_task_lock.release(flags);
}

/** @brief Unschedule the task.
 *
 * After unschedule() returns, the task will not run until it is rescheduled
 * with reschedule().
 *
 * @note scheduled() may return true for a short time even after unschedule().
 *
 * @sa reschedule, strong_unschedule, fast_unschedule
 */
void
Task::unschedule()
{
    // Thanksgiving 2001: unschedule() will always unschedule the task. This
    // seems more reliable, since some people depend on unschedule() ensuring
    // that the task is not scheduled any more, no way, no how. Possible
    // problem: calling unschedule() from run_task() will hang!
    // 2005: It shouldn't hang.
    // 2008: Allow unschedule() in interrupt context.
    bool done = false;
    _should_be_scheduled = false;
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    if (unlikely(_thread == 0))
	done = true;
#if CLICK_LINUXMODULE
    else if (in_interrupt())
	goto pending;
#endif
    else if (attempt_lock_tasks()) {
	fast_unschedule(false);
	_thread->unlock_tasks();
	done = true;
    }
#if CLICK_LINUXMODULE
  pending:
#endif
    if (!done)
	add_pending();
}

#if HAVE_TASK_HEAP
void
Task::fast_reschedule()
{
    // should not be scheduled at this point
    assert(_thread);
#if CLICK_LINUXMODULE
    // tasks never run at interrupt time
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    // GIANT_REQUIRED;
#endif

    if (!scheduled()) {
	// increase pass
	_pass += _stride;

	if (_thread->_task_heap_hole) {
	    _schedpos = 0;
	    _thread->_task_heap_hole = 0;
	} else {
	    _schedpos = _thread->_task_heap.size();
	    _thread->_task_heap.push_back(0);
	}
	_thread->task_reheapify_from(_schedpos, this);
    }
}
#endif

void
Task::true_reschedule()
{
    bool done = false;
    _should_be_scheduled = true;
    if (unlikely(_thread == 0))
	done = true;
#if CLICK_LINUXMODULE
    else if (in_interrupt())
	goto pending;
#endif
    else if (attempt_lock_tasks()) {
	Router *router = _owner->router();
	if (router->_running >= Router::RUNNING_BACKGROUND) {
	    if (!scheduled() && _should_be_scheduled) {
		fast_schedule();
		_thread->wake();
	    }
	    done = true;
	}
	_thread->unlock_tasks();
    }
#if CLICK_LINUXMODULE
  pending:
#endif
    if (!done)
	add_pending();
}

/** @brief Unschedule the Task and move it to a quiescent thread.
 *
 * When strong_unschedule() returns, the task will not be scheduled on any
 * thread.  Furthermore, the task has been moved to a temporary "dead" thread.
 * Future reschedule() calls will not schedule the task, since the dead thread
 * never runs; future move_thread() calls change the home thread ID,
 * but leave the thread on the dead thread.  Only strong_reschedule() can make
 * the task run again.
 * @sa strong_reschedule, unschedule
 */
void
Task::strong_unschedule()
{
    bool done = false;
    _should_be_strong_unscheduled = true;
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    // unschedule() and move to a quiescent thread, so that subsequent
    // reschedule()s won't have any effect
    if (unlikely(_thread == 0))
	done = true;
#if CLICK_LINUXMODULE
    else if (in_interrupt())
	goto pending;
#endif
    else if (attempt_lock_tasks()) {
	fast_unschedule(false);
	Master *m = _owner->master();
	RouterThread *old_thread = _thread;
	_thread = m->thread(RouterThread::THREAD_STRONG_UNSCHEDULE);
	old_thread->unlock_tasks();
	done = true;
    }
#if CLICK_LINUXMODULE
  pending:
#endif
    if (!done)
	add_pending();
}

/** @brief Reschedule the Task, moving it from the "dead" thread to its home
 * thread if appropriate.
 *
 * This function undoes any previous strong_unschedule().  If the task is on
 * the "dead" thread, then it is moved to its home thread.  The task is also
 * rescheduled.  Due to locking issues, the task may not be scheduled right
 * away -- scheduled() may not immediately return true.
 *
 * @sa reschedule, strong_unschedule
 */
void
Task::strong_reschedule()
{
    bool done = false;
    _should_be_scheduled = true;
    _should_be_strong_unscheduled = false;
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    if (unlikely(_thread == 0))
	done = true;
#if CLICK_LINUXMODULE
    else if (in_interrupt())
	goto pending;
#endif
    else if (attempt_lock_tasks()) {
	RouterThread *old_thread = _thread;
	if (old_thread->thread_id() == RouterThread::THREAD_STRONG_UNSCHEDULE) {
	    fast_unschedule(true);
	    _thread = _owner->master()->thread(_home_thread_id);
	} else if (old_thread->thread_id() == _home_thread_id) {
	    _should_be_scheduled = true;
	    fast_reschedule();
	    done = true;
	} else {
	    // This task must already be on the pending list so it can be moved.
	    _should_be_scheduled = true;
	    done = true;
	}
	old_thread->unlock_tasks();
    }
#if CLICK_LINUXMODULE
  pending:
#endif
    if (!done)
	add_pending();
}

/** @brief Move the Task to a new home thread.
 *
 * The home thread ID is set to @a thread_id.  The task, if it is currently
 * scheduled, is rescheduled on thread @a thread_id; this generally takes some
 * time to take effect.  @a thread_id can be less than zero, in which case the
 * thread is scheduled on a quiescent thread: it will never be run.
 */
void
Task::move_thread(int thread_id)
{
    bool done = false;
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    if (thread_id < RouterThread::THREAD_QUIESCENT)
	thread_id = RouterThread::THREAD_QUIESCENT;
    _home_thread_id = thread_id;

    if (unlikely(_thread == 0))
	done = true;
#if CLICK_LINUXMODULE
    else if (in_interrupt())
	goto pending;
#endif
    else if (attempt_lock_tasks()) {
	RouterThread *old_thread = _thread;
	if (old_thread->thread_id() != _home_thread_id
	    && old_thread->thread_id() != RouterThread::THREAD_STRONG_UNSCHEDULE
	    && !_should_be_strong_unscheduled) {
	    if (scheduled())
		fast_unschedule(true);
	    _thread = _owner->master()->thread(_home_thread_id);
	    old_thread->unlock_tasks();
	    if (_should_be_scheduled)
		add_pending();
	} else
	    old_thread->unlock_tasks();
	done = true;
    }
#if CLICK_LINUXMODULE
  pending:
#endif
    if (!done)
	add_pending();
}

void
Task::process_pending(RouterThread *thread)
{
    // Must be called with thread->lock held.
    // May be called in the process of destroying router(), so must check
    // router()->_running when necessary.  (Not necessary for add_pending()
    // since that function does it already.)

#if CLICK_BSDMODULE
    int s = splimp();
#endif

    // The quiescent threads, which have thread_id() < 0, never run, and so
    // they never call process_pending().  Process their tasks as well.
    RouterThread *other_thread = 0;
    if (_thread && _thread != thread && _thread->thread_id() < 0) {
	other_thread = _thread;
	other_thread->lock_tasks();
	goto change_schedule;
    }

    if (_thread == thread) {
	// we know the thread is locked.
	// see also move_thread() above
      change_schedule:
	int want_thread_id;
	if (_should_be_strong_unscheduled)
	    want_thread_id = RouterThread::THREAD_STRONG_UNSCHEDULE;
	else
	    want_thread_id = _home_thread_id;

	if (_thread->thread_id() != want_thread_id) {
	    if (scheduled())
		fast_unschedule(true);
	    _thread = _owner->master()->thread(want_thread_id);
	} else if (_should_be_scheduled && _owner->router()->running()) {
	    if (!scheduled())
		fast_schedule();
	}
    }

    if (_should_be_scheduled && !_should_be_strong_unscheduled && !scheduled())
	add_pending();

    if (other_thread)
	other_thread->unlock_tasks();

#if CLICK_BSDMODULE
    splx(s);
#endif
}

CLICK_ENDDECLS

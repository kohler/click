// -*- c-basic-offset: 4; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2005 Regents of the University of California
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

/** @class Task
 * @brief Represents a frequently-scheduled computational task.
 *
 * Click schedules a router's CPU or CPUs with one or more <em>task
 * queues</em>.  These queues are simply lists of @e tasks, which represent
 * functions that would like unconditional access to the CPU.  Tasks are
 * generally associated with elements.  When scheduled, most tasks call some
 * element's @link Element::run_task() run_task()@endlink method.
 *
 * Click tasks are represented by Task objects.  An element that would like
 * special access to a router's CPU should include and initialize a Task
 * instance variable.
 *
 * Tasks are called very frequently, up to tens of thousands of times per
 * second.  Unlike Timer objects and selections, tasks are called
 * unconditionally.  Elements generally use Tasks for frequent tasks, and
 * implement their own algorithms for scheduling and unscheduling the tasks
 * when there's work to be done.  For infrequent events, it is far more
 * efficient to use Timer objects.
 *
 * Since Click tasks are cooperatively scheduled, executing a task should not
 * take a long time.  Very long tasks can inappropriately delay timers and
 * other periodic events.  We may address this problem in a future release,
 * but for now, keep tasks short.
 */

// - Changes to _thread are protected by _thread->lock.
// - Changes to _home_thread_id are protected by
//   _router->master()->task_lock.
// - If _pending is nonzero, then _pending_next is nonnull.
// - Either _home_thread_id == _thread->thread_id(), or
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
#if HAVE_TASK_HEAP
    if (scheduled() || _pending)
	cleanup();
#else
    if ((scheduled() || _pending) && _thread != this)
	cleanup();
#endif
}

/** @brief Return the master where this task will be scheduled.
 */
Master *
Task::master() const
{
    assert(_thread);
    return _thread->master();
}

/** @brief Initialize the Task, and optionally schedule it.
 * @param router the router containing this Task
 * @param schedule if true, the Task will be scheduled immediately
 *
 * This function must be called on every Task before it is used.  The @a
 * router's ThreadSched, if any, is used to determine the task's initial
 * thread assignment.  The task initially has the default number of tickets,
 * and is scheduled iff @a schedule is true.
 *
 * An assertion will fail if a Task is initialized twice.
 */
void
Task::initialize(Router *router, bool schedule)
{
    assert(!initialized() && !scheduled());

    _home_thread_id = router->initial_home_thread_id(this, schedule);
    if (_home_thread_id == ThreadSched::THREAD_UNKNOWN)
	_home_thread_id = 0;
    // Master::thread() returns the quiescent thread if its argument is out of
    // range
    _thread = router->master()->thread(_home_thread_id);

    // set _router last, since it is used to determine whether task is
    // initialized
    _router = router;
    
#ifdef HAVE_STRIDE_SCHED
    set_tickets(DEFAULT_TICKETS);
#endif

    if (schedule)
	add_pending(RESCHEDULE);
}

/** @brief Initialize the Task, and optionally schedule it.
 * @param e specifies the router containing the Task
 * @param schedule if true, the Task will be scheduled immediately
 *
 * This method is shorthand for @link Task::initialize(Router *, bool) initialize@endlink(@a e ->@link Element::router router@endlink(), bool).
 */
void
Task::initialize(Element *e, bool schedule)
{
    initialize(e->router(), schedule);
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
    if (_router->_running >= Router::RUNNING_PREPARING) {
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

/** @brief Unschedules the task.
 *
 * When unschedule() returns, the task will not be scheduled on any thread.
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
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    assert(!intr_nesting_level);
    SPLCHECK;
#endif
    if (_thread) {
	lock_tasks();
	fast_unschedule();
	_pending &= ~RESCHEDULE;
	_thread->unlock_tasks();
    }
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
    // assert(!intr_nesting_level); it happens all the time from fromdevice!
    SPLCHECK;
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
    assert(_thread);
    bool done = false;
#if CLICK_LINUXMODULE
    if (in_interrupt())
	goto skip_lock;
#endif
#if CLICK_BSDMODULE
    SPLCHECK;
#endif
    if (attempt_lock_tasks()) {
	if (_router->_running >= Router::RUNNING_BACKGROUND) {
	    if (!scheduled()) {
		fast_schedule();
		_thread->wake();
	    }
	    done = true;
	}
	_thread->unlock_tasks();
    }
#if CLICK_LINUXMODULE
  skip_lock:
#endif
    if (!done)
	add_pending(RESCHEDULE);
}

/** @brief Unschedules the Task and moves it to a quiescent thread.
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
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    assert(!intr_nesting_level);
    SPLCHECK;
#endif
    // unschedule() and move to a quiescent thread, so that subsequent
    // reschedule()s won't have any effect
    if (_thread) {
	lock_tasks();
	fast_unschedule();
	RouterThread *old_thread = _thread;
	_pending &= ~(RESCHEDULE | CHANGE_THREAD);
	_thread = _router->master()->thread(RouterThread::THREAD_STRONG_UNSCHEDULE);
	old_thread->unlock_tasks();
    }
}

/** @brief Reschedules the Task, moving it from the "dead" thread to its home
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
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    assert(!intr_nesting_level);
    SPLCHECK;
#endif
    assert(_thread);
    lock_tasks();
    RouterThread *old_thread = _thread;
    if (old_thread->thread_id() == RouterThread::THREAD_STRONG_UNSCHEDULE) {
	fast_unschedule();
	_thread = _router->master()->thread(_home_thread_id);
	add_pending(RESCHEDULE);
    } else if (old_thread->thread_id() == _home_thread_id)
	fast_reschedule();
    old_thread->unlock_tasks();
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
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    assert(!intr_nesting_level);
    SPLCHECK;
#endif
    if (thread_id < RouterThread::THREAD_QUIESCENT)
	thread_id = RouterThread::THREAD_QUIESCENT;
    _home_thread_id = thread_id;

    if (attempt_lock_tasks()) {
	RouterThread *old_thread = _thread;
	if (old_thread->thread_id() != _home_thread_id
	    && old_thread->thread_id() != RouterThread::THREAD_STRONG_UNSCHEDULE) {
	    if (scheduled()) {
		fast_unschedule();
		_pending |= RESCHEDULE;
	    }
	    _thread = _router->master()->thread(_home_thread_id);
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

#if CLICK_BSDMODULE
    int s = splimp();
#endif

    if (_thread == thread) {
	if (_pending & CHANGE_THREAD) {
	    // see also move_thread() above
	    _pending &= ~CHANGE_THREAD;
	    if (_thread->thread_id() != _home_thread_id
		&& _thread->thread_id() != RouterThread::THREAD_STRONG_UNSCHEDULE) {
		if (scheduled()) {
		    fast_unschedule();
		    _pending |= RESCHEDULE;
		}
		_thread = _router->master()->thread(_home_thread_id);
	    }
	} else if (_pending & RESCHEDULE) {
	    _pending &= ~RESCHEDULE;
	    if (!scheduled())
		fast_schedule();
	}
    }

    if (_pending)
	add_pending(0);

#if CLICK_BSDMODULE
    splx(s);
#endif
}

CLICK_ENDDECLS

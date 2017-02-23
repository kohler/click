// -*- c-basic-offset: 4; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
 * Copyright (c) 1999-2017 Eddie Kohler
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

// Lock protection:
// - _status, which contains home_thread_id, is_scheduled, and
//   is_strong_unscheduled, may be changed at any time without locking.
// - Changes to _thread are protected by old(_thread)->_pending_lock.
//   Furthermore, only old(_thread) itself may change _thread
//   (except that if old(_thread) is quiescent, anyone may change _thread).
// - _pending_nextptr is generally protected by _thread->_pending_lock.
//   It may be set by another thread only if it is 0.
//   After acquiring _thread->_pending_lock, verify that _thread has not
//   changed.
// - _thread may be read at any time. However, if it is being read from a
//   different thread, it might change underneath. Read it into a local
//   variable. To arrange for _thread to change, set home_thread_id and
//   add_pending().

// Invariants:
// - If is_scheduled && !is_strong_unscheduled && not on quiescent thread,
//   then on_scheduled_list() || on_pending_list(). (The invariant is
//   temporarily violated during the operation of schedule(), which sets
//   is_scheduled then manipulates lists.)
// - If on quiescent thread, then !on_scheduled_list() && !on_pending_list().
//   (It is possible to transiently have _thread quiescent && on a list, if
//   the task's thread is being changed.)
// - If on_scheduled_list(), then _thread owns the relevant scheduled list.
// - _pending_nextptr may be 0 (not on pending list), 1 (currently being
//   processed by Task::process_pending: a temporary state), 2 (at end of
//   list), >2 (in middle of list).

bool
Task::error_hook(Task *, void *)
{
    assert(0);
    return false;
}

Task::~Task()
{
    if (needs_cleanup())
        cleanup();
}

Master *
Task::master() const
{
    assert(_thread);
    return _thread->master();
}


void
Task::complete_schedule(RouterThread* process_pending_thread)
{
    RouterThread* thread = _thread;
    if (!thread)
        return;

    // If called from another thread, or task's router isn't running,
    // add_pending instead.
    if ((process_pending_thread
         ? process_pending_thread != thread
         : !thread->current_thread_is_running())
        || !router()->running()) {
        if (_pending_nextptr.x < 2)
            add_pending(process_pending_thread != 0);
        return;
    }

    // If we get here, we know that thread->task_lock() is held by the
    // current thread.

    // Skip if already scheduled.
    if (on_scheduled_list())
        goto done;

#if CLICK_LINUXMODULE
    // tasks never run at interrupt time in Linux
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif

#if HAVE_STRIDE_SCHED
    // update pass
    _pass = thread->pass() + _stride;

# if HAVE_TASK_HEAP
    _schedpos = thread->_task_heap.size();
    thread->_task_heap.push_back(RouterThread::task_heap_element());
    thread->task_reheapify_from(_schedpos, this);
# elif 0
    {
        // look for 'n' immediately before where we should be scheduled
        TaskLink *n = thread->_prev;
        while (n != thread && PASS_GT(n->_pass, _pass))
            n = n->_prev;
        // schedule after 'n'
        _next = n->_next;
        _prev = n;
        n->_next = this;
        _next->_prev = this;
    }
# else
    {
        // look for 'n' immediately after where we should be scheduled
        TaskLink *n = thread->_task_link._next;
        while (n != &thread->_task_link && !PASS_GT(n->_pass, _pass))
            n = n->_next;
        // schedule before 'n'
        _prev = n->_prev;
        _next = n;
        n->_prev = this;
        _prev->_next = this;
    }
# endif

#else /* !HAVE_STRIDE_SCHED */
    // schedule at the end of the list
    _prev = thread->_task_link._prev;
    _next = &thread->_task_link;
    thread->_task_link._prev = this;
    _prev->_next = this;
#endif /* HAVE_STRIDE_SCHED */

 done:
    if (process_pending_thread) {
        click_fence();
        _pending_nextptr.x = 0;
    }
}


void
Task::add_pending(bool always)
{
    // lock current thread and wait for current process_pending to complete
    // (indicated by _pending_nextptr.x == 1)
    RouterThread* thread;
    SpinlockIRQ::flags_t flags;
    while (1) {
        thread = _thread;
        flags = thread->_pending_lock.acquire();
        if (thread == _thread && (_pending_nextptr.x != 1 || always))
            break;
        thread->_pending_lock.release(flags);
    }

    // quiescent threads have no pending list; if current thread is
    // quiescent, but task has been moved, change threads now
    if (thread->thread_id() < 0 && _status.home_thread_id >= 0) {
        assert(!on_scheduled_list() && !on_pending_list());
        RouterThread* next_thread = thread->master()->thread(_status.home_thread_id);
        if (next_thread != thread) {
            // No deadlock: lock order by thread_id()
            SpinlockIRQ::flags_t next_flags = next_thread->_pending_lock.acquire();
            _thread = next_thread;
            thread->_pending_lock.release(flags);
            thread = next_thread;
            flags = next_flags;
        }
    }

    // add to list, unless the router is in the process of dying
    if (_pending_nextptr.x < 2) {
        assert(_pending_nextptr.x == 0 || always);
        if (thread->thread_id() >= 0 && !router()->dying()) {
            _pending_nextptr.x = 2;
            thread->_pending_tail->t = this;
            thread->_pending_tail = &_pending_nextptr;
            thread->add_pending();
        } else
            _pending_nextptr.x = 0;
    }

    thread->_pending_lock.release(flags);
}


void
Task::initialize(Element *owner, bool schedule)
{
    assert(owner && !initialized() && !scheduled());

    Router *router = owner->router();
    int tid = _status.home_thread_id;
    if (tid == -2)
        tid = router->home_thread_id(owner);
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
        add_pending(false);
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
        // Move the task to a quiescent thread.
        _status.home_thread_id = -1;
        click_fence();

        // Task must not be scheduled. If scheduled on another thread, wait
        // for that thread to notice. If scheduled on this thread, remove
        // it ourselves.

        // Task must not be on any pending list. If on another thread, wait
        // for that thread to notice. If on this thread, we must remove it
        // ourselves.

        while (needs_cleanup()) {
            RouterThread* thread = _thread;
            if (!thread->current_thread_is_running_cleanup()) {
                click_relax_fence();
                continue;
            }

            if (on_scheduled_list()) {
                remove_from_scheduled_list();
                click_fence();
                continue;
            }

            SpinlockIRQ::flags_t flags = thread->_pending_lock.acquire();
            if (thread == _thread && on_pending_list()) {
                Pending *tptr = &thread->_pending_head;
                while (tptr->x > 2 && tptr->t != this)
                    tptr = &tptr->t->_pending_nextptr;
                // We'll usually have `tptr->t == this`; but if another
                // thread's `Task::process_pending()` has just set
                // `this->_thread` to this thread (because of an earlier
                // move_thread), we might have `tptr->x == 1` (because the
                // task is on the other thread's pending list still).
                if (tptr->t == this) {
                    *tptr = _pending_nextptr;
                    if (_pending_nextptr.x == 2) {
                        thread->_pending_tail = tptr;
                        if (tptr == &thread->_pending_head)
                            tptr->x = 0;
                    }
                    _pending_nextptr.x = 0;
                }
            }
            thread->_pending_lock.release(flags);
        }

        _owner = 0;
        _thread = 0;
    }
}

void
Task::move_thread(int new_thread_id)
{
    _status.home_thread_id = new_thread_id;
    click_fence();
    Task::Status status(_status);
    if (likely(_thread != 0)
        && status.home_thread_id >= 0
        && status.is_scheduled
        && !status.is_strong_unscheduled
        && _pending_nextptr.x < 2)
        add_pending(false);
}

void
Task::process_pending(RouterThread* thread)
{
    // Must be called with thread->task_lock held.
    // May be called from thread->kill_router(), in which case
    // !thread->current_thread_is_running().
    assert(thread == _thread);

    // Mark this task as being currently processed.
    _pending_nextptr.x = 1;
    click_fence();

    Task::Status status(_status);
    if (status.home_thread_id != thread->thread_id()) {
        SpinlockIRQ::flags_t flags = thread->_pending_lock.acquire();
        remove_from_scheduled_list();
        click_fence();
        _thread = thread->master()->thread(status.home_thread_id);
        thread->_pending_lock.release(flags);
    }

    if (status.is_scheduled && !status.is_strong_unscheduled)
        complete_schedule(thread);
    else
        _pending_nextptr.x = 0;
}

CLICK_ENDDECLS

// -*- c-basic-offset: 2; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
CLICK_DECLS

// - Access to _thread is protected by _lock and _thread->lock.
// - If _pending is nonzero, then _pending_next is nonnull.
// - Either _thread_preference == _thread->thread_id(), or
//   _thread->thread_id() == -1.

void
Task::error_hook(Task *, void *)
{
  assert(0);
}

Task::~Task()
{
  assert(!scheduled() || _thread == this);
}

void
Task::initialize(Router *r, bool join)
{
  assert(!initialized() && !scheduled());

  TaskList *tl = r->task_list();
  tl->lock();
  
  _all_list = tl;
  _all_prev = tl;
  _all_next = tl->_all_next;
  tl->_all_next = _all_next->_all_prev = this;

  _thread = r->thread(0);
  _thread_preference = 0;
#ifdef HAVE_STRIDE_SCHED
  set_tickets(DEFAULT_TICKETS);
#endif
  
  tl->unlock();

  if (join)
    reschedule();
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
    unschedule();
    
    _all_list->lock();

    _all_prev->_all_next = _all_next; 
    _all_next->_all_prev = _all_prev; 
    _all_next = _all_prev = _all_list = 0;
    _thread = 0;
    
    _all_list->unlock();
  }
}

void
Task::add_pending(int p)
{
  _all_list->lock();
  _pending |= p;
  if (!_pending_next && _pending) {
    _pending_next = _all_list->_pending_next;
    _all_list->_pending_next = this;
  }
  if (_pending)
    _thread->add_pending();
  _all_list->unlock();
}

void
Task::unschedule()
{
  // Thanksgiving 2001: unschedule() will always unschedule the task. This
  // seems more reliable, since some people depend on unschedule() ensuring
  // that the task is not scheduled any more, no way, no how. Possible
  // problem: calling unschedule() from run_scheduled() will hang!
  _lock.acquire();
  
  if (_thread) {
    _thread->lock_tasks();
    fast_unschedule();
    _pending &= ~RESCHEDULE;
    _thread->unlock_tasks();
  }
  
  _lock.release();
}

void
Task::reschedule()
{
  _lock.acquire();
  
  assert(_thread);
  if (_thread->attempt_lock_tasks()) {
    if (!scheduled())
      fast_schedule();
    _thread->unlock_tasks();
  } else
    add_pending(RESCHEDULE);
  
  _lock.release();
}

void
Task::strong_unschedule()
{
  // unschedule() and move to the quiescent thread, so that subsequent
  // reschedule()s won't have any effect
  _lock.acquire();
  
  if (RouterThread *old_thread = _thread) {
    old_thread->lock_tasks();
    fast_unschedule();
    _thread = old_thread->router()->thread(-1);
    _pending &= ~(RESCHEDULE | CHANGE_THREAD);
    old_thread->unlock_tasks();
  }
  
  _lock.release();
}

void
Task::strong_reschedule()
{
  _lock.acquire();
  
  assert(_thread);
  RouterThread *old_thread = _thread;
  old_thread->lock_tasks();
  fast_unschedule();
  _thread = old_thread->router()->thread(_thread_preference);
  add_pending(RESCHEDULE);
  old_thread->unlock_tasks();
  
  _lock.release();
}

void
Task::change_thread(int new_preference)
{
  _lock.acquire();

  assert(_thread);
  RouterThread *old_thread = _thread;
  Router *router = _thread->router();
  int old_preference = _thread_preference;
  _thread_preference = new_preference;
  if (_thread_preference < 0 || _thread_preference >= router->nthreads())
    _thread_preference = -1;	// quiescent thread

  if (_thread_preference == old_preference
      || old_preference != old_thread->thread_id())
    /* nothing to do */;
  else if (old_thread->attempt_lock_tasks()) {
    // see also process_pending() below
    if (scheduled()) {
      fast_unschedule();
      _pending |= RESCHEDULE;
    }
    _thread = router->thread(_thread_preference);
    old_thread->unlock_tasks();
  } else
    _pending |= CHANGE_THREAD;

  if (_pending)
    add_pending(0);

  _lock.release();
}

void
Task::process_pending(RouterThread *thread)
{
  _lock.acquire();

  if (_thread == thread) {
    if (_pending & CHANGE_THREAD) {
      // see also change_thread() above
      _pending &= ~CHANGE_THREAD;
      if (scheduled()) {
	fast_unschedule();
	_pending |= RESCHEDULE;
      }
      _thread = thread->router()->thread(_thread_preference);
    } else if (_pending & RESCHEDULE) {
      _pending &= ~RESCHEDULE;
      if (!scheduled())
	fast_schedule();
    }
  }

  if (_pending)
    add_pending(0);

  _lock.release();
}

TaskList::TaskList()
  : Task(Task::error_hook, 0)
{
  _pending_next = _all_prev = _all_next = _all_list = this;
}

CLICK_ENDDECLS

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

// - Changes to _thread are protected by _thread->lock.
// - Changes to _thread_preference are protected by _all_list->lock.
// - If _pending is nonzero, then _pending_next is nonnull.
// - Either _thread_preference == _thread->thread_id(), or
//   _thread->thread_id() == -1.

bool
Task::error_hook(Task *, void *)
{
  assert(0);
  return false;
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
  // problem: calling unschedule() from run_task() will hang!
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
  if (attempt_lock_tasks()) {
    if (!scheduled())
      fast_schedule();
    _thread->unlock_tasks();
  } else
    add_pending(RESCHEDULE);
}

void
Task::strong_unschedule()
{
  // unschedule() and move to the quiescent thread, so that subsequent
  // reschedule()s won't have any effect
  if (_thread) {
    lock_tasks();
    fast_unschedule();
    RouterThread *old_thread = _thread;
    _pending &= ~(RESCHEDULE | CHANGE_THREAD);
    _thread = old_thread->router()->thread(-1);
    old_thread->unlock_tasks();
  }
}

void
Task::strong_reschedule()
{
  assert(_thread);
  lock_tasks();
  fast_unschedule();
  RouterThread *old_thread = _thread;
  _thread = old_thread->router()->thread(_thread_preference);
  add_pending(RESCHEDULE);
  old_thread->unlock_tasks();
}

void
Task::change_thread(int new_preference)
{
  Router *router = _thread->router();
  _thread_preference = new_preference;
  if (_thread_preference < 0 || _thread_preference >= router->nthreads())
    _thread_preference = -1;	// quiescent thread

  if (attempt_lock_tasks()) {
    RouterThread *old_thread = _thread;
    if (_thread_preference != old_thread->thread_id()) {
      if (scheduled()) {
	fast_unschedule();
	_pending |= RESCHEDULE;
      }
      _thread = router->thread(_thread_preference);
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
      _thread = thread->router()->thread(_thread_preference);
    } else if (_pending & RESCHEDULE) {
      _pending &= ~RESCHEDULE;
      if (!scheduled())
	fast_schedule();
    }
  }

  if (_pending)
    add_pending(0);
}

TaskList::TaskList()
  : Task(Task::error_hook, 0)
{
  _pending_next = _all_prev = _all_next = _all_list = this;
}

CLICK_ENDDECLS

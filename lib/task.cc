// -*- c-basic-offset: 2; related-file-name: "../include/click/task.hh" -*-
/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
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

#include <click/task.hh>
#include <click/router.hh>
#include <click/routerthread.hh>

void
Task::error_hook(Task *, void *)
{
  assert(0);
}

Task::~Task()
{
  assert(!scheduled() || _list == this);
  //assert(!initialized());
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

  _list = r->thread(0);
#if __MTCLICK__
  _thread_preference = _list->thread_id();
#endif
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
Task::uninitialize()
{
  if (initialized()) {
    unschedule();
    
    _all_list->lock();

    _all_prev->_all_next = _all_next; 
    _all_next->_all_prev = _all_prev; 
    _all_next = _all_prev = _all_list = 0;
    _list = 0;
    
    _all_list->unlock();
  }
}

#if __MTCLICK__
void
Task::change_thread(int thread_id)
{
  assert(_list);
  RouterThread *old_list = _list;
  Router *router = old_list->router();

  if (thread_id < 0 || thread_id >= router->nthreads())
    thread_id = -1;		// quiescent thread
  
  int old_preference = _thread_preference;
  if (old_preference != thread_id)
    _thread_preference = thread_id;

  if (thread_id == old_list->thread_id())
    /* remaining on same thread; do nothing */;
  else if (old_list->attempt_lock_tasks()) {
    bool was_scheduled = scheduled();
    if (was_scheduled)
      fast_unschedule();
    _list = router->thread(thread_id);
    old_list->unlock_tasks();
    if (was_scheduled)
      reschedule();
  } else
    old_list->add_task_request(RouterThread::MOVE_TASK, this);
}

void
Task::fast_change_thread()
{
  // called with _list locked
  assert(_list);
  RouterThread *old_list = _list;
  Router *router = old_list->router();
  int thread_id = _thread_preference;
  
  if (thread_id == old_list->thread_id())
    /* remaining on same thread; do nothing */;
  else {
    bool was_scheduled = scheduled();
    if (was_scheduled)
      fast_unschedule();
    _list = router->thread(thread_id);
    if (was_scheduled)
      reschedule();
  }
}
#endif

void
Task::reschedule()
{
  assert(_list);
  if (_list->attempt_lock_tasks()) {
    if (!scheduled()) {
#ifdef HAVE_STRIDE_SCHED
      if (_tickets >= 1) {
	_pass = _list->_next->_pass;
	fast_reschedule();
      }
#else
      fast_reschedule();
#endif
    }
    _list->unlock_tasks();
  } else
    _list->add_task_request(RouterThread::SCHEDULE_TASK, this);
}

void
Task::unschedule()
{
  // Thanksgiving 2001: unschedule() will always unschedule the task. This
  // seems more reliable, since some people depend on unschedule() ensuring
  // that the task is not scheduled any more, no way, no how. Possible
  // problem: calling unschedule() from run_scheduled() will hang!
  if (_list) {
    _list->lock_tasks();
    fast_unschedule();
    _list->unlock_tasks();
  }
}

void
Task::unschedule_soon()
{
  if (_list) {
    if (_list->attempt_lock_tasks()) {
      fast_unschedule();
      _list->unlock_tasks();
    } else
      _list->add_task_request(RouterThread::UNSCHEDULE_TASK, this);
  }
}

TaskList::TaskList()
  : Task(Task::error_hook, 0)
{
  _all_prev = _all_next = _all_list = this;
}


/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/task.hh>
#include <click/router.hh>
#include <click/routerthread.hh>

#ifndef RR_SCHED
void
Task::set_max_tickets(int n)
{
  if (n > MAX_TICKETS)
    n = MAX_TICKETS;
  _max_tickets = n;
}
#endif

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
  set_thread_preference(_list->thread_id());
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
  assert(!scheduled());
  if (initialized()) {
    _all_list->lock();

    _all_prev->_all_next = _all_next; 
    _all_next->_all_prev = _all_prev; 
    _all_next = _all_prev = _all_list = 0;
    _list = 0;
    
    _all_list->unlock();
  }
}

void
Task::join_scheduler(RouterThread *rt)
{
  assert(initialized() && !scheduled());

  _all_list->lock();
  _list = rt;
#ifdef __MTCLICK__
  set_thread_preference(rt->thread_id());
#endif
  _all_list->unlock();

  reschedule();
}

void
Task::reschedule()
{
  assert(have_scheduler());
  if (_list->attempt_lock_tasks()) {
#ifndef RR_SCHED
    if (_tickets >= 1) {
      _pass = _list->_next->_pass;
      fast_reschedule();
    }
#else
    fast_reschedule();
#endif
    _list->unlock_tasks();
  } else
    _list->add_task_request(RouterThread::SCHEDULE_TASK, this);
}

void
Task::unschedule()
{
  if (scheduled()) {
    if (_list->attempt_lock_tasks()) {
      fast_unschedule();
      _list->unlock_tasks();
    } else
      _list->add_task_request(RouterThread::UNSCHEDULE_TASK, this);
  }
}

TaskList::TaskList()
{
  _all_prev = _all_next = _all_list = this;
}

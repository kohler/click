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
Task::initialize(TaskList *t)
{
  assert(!scheduled());
  uninitialize();
  _all_prev = t;
  _all_next = t->_all_next;
  t->_all_next = _all_next->_all_prev = this;
}

void
Task::initialize(Element *e)
{
  initialize(e->router());
}

void
Task::initialize(Router *r)
{
  initialize(r->task_list());
}

void
Task::uninitialize()
{
  assert(!scheduled());
  if (initialized()) {
    _all_prev->_all_next = _all_next;
    _all_next->_all_prev = _all_prev;
    _all_prev = _all_next;
  }
}


void
Task::reschedule()
{
  _list->lock();
#ifndef RR_SCHED
  if (_tickets >= 1) {
    _pass = _list->_next->_pass;
    fast_reschedule();
  }
#else
  fast_reschedule();
#endif
  _list->unlock();
}

void
Task::unschedule()
{
  _list->lock();
  fast_unschedule();
  _list->unlock();
}

void
Task::schedule_immediately()
{
  assert(!_next);

  _list->lock();
  _next = _list->_next;
  _prev = _list;
  _list->_next = this;
  _next->_prev = this;
  _list->unlock();
}


void
Task::join_scheduler(Router *r)
{
  join_scheduler(r->task_list());
}


TaskList::TaskList()
{
  _prev = _next = _list = this;
  _all_prev = _all_next = this;
}

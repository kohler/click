/*
 * task.{cc,hh} -- a linked list of schedulable entities
 * Eddie Kohler
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

inline void
Task::join_scheduler()
{
  assert(_list);
  if (_tickets < 1 || scheduled())
    return;
  if (_list->_next == _list)
    /* nothing on worklist */
    _pass = 0;
  else 
    _pass = _list->_next->_pass;
  reschedule();
}

#else

inline void
Task::join_scheduler()
{
  assert(_list);
  if (!scheduled())
    reschedule();
}

#endif

inline void
Task::join_scheduler(Task *task_list)
{
  assert(!_prev && !_next && task_list->is_list());
  _list = task_list;
  join_scheduler();
}

void
Task::join_scheduler(Router *r)
{
  join_scheduler(r->task_list());
}

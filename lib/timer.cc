/*
 * timer.{cc,hh} -- portable timers. Linux kernel module uses Linux timers
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
#include <click/timer.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/task.hh>

Timer::Timer()
  : _prev(this), _next(this), _hook(head_hook), _thunk(0), _head(this)
{
}

Timer::Timer(Timer *head, TimerHook hook, void *thunk)
  : _prev(0), _next(0), _hook(hook), _thunk(thunk), _head(head)
{
  assert(_head->is_head());
}

Timer::Timer(TimerHook hook, void *thunk)
  : _prev(0), _next(0), _hook(hook), _thunk(thunk), _head(0)
{
}

Timer::Timer(Element *e)
  : _prev(0), _next(0), _hook(element_hook), _thunk(e), _head(0)
{
}

Timer::Timer(Task *t)
  : _prev(0), _next(0), _hook(task_hook), _thunk(t), _head(0)
{
}

void
Timer::head_hook(Timer *, void *)
{
  assert(0);
}

/*
 * Timer::element_hook is a callback that gets called when a Timer,
 * constructed with just an Element instance, expires. 
 * 
 * When used in userlevel or kernel polling mode, timer is maintained by
 * Click, so Timer::element_hook is called within Click.
 */

void
Timer::element_hook(Timer *, void *thunk)
{
  Element *e = (Element *)thunk;
  e->run_scheduled();
}

void
Timer::task_hook(Timer *, void *thunk)
{
  Task *task = (Task *)thunk;
  task->schedule_immediately();
}

void
Timer::initialize(Router *r)
{
  initialize(r->timer_head());
}

void
Timer::initialize(Element *e)
{
  initialize(e->router()->timer_head());
}

inline void
Timer::finish_schedule()
{
  Timer *prev = _head;
  Timer *trav = prev->_next;
  while (trav != _head && timercmp(&_expires, &trav->_expires, >)) {
    prev = trav;
    trav = trav->_next;
  }
  _prev = prev;
  _next = trav;
  _prev->_next = this;
  trav->_prev = this;
}

void
Timer::schedule_at(const struct timeval &when)
{
  assert(!is_head() && initialized());
  if (scheduled())
    unschedule();
  _expires = when;
  finish_schedule();
}

void
Timer::schedule_after_ms(int ms)
{
  assert(!is_head() && initialized());
  if (scheduled())
    unschedule();
  click_gettimeofday(&_expires);
  struct timeval interval;
  interval.tv_sec = ms / 1000;
  interval.tv_usec = (ms % 1000) * 1000;
  timeradd(&_expires, &interval, &_expires);
  finish_schedule();
}

void
Timer::unschedule()
{
  if (scheduled()) {
    _prev->_next = _next;
    _next->_prev = _prev;
    _prev = _next = 0;
  }
}

void
Timer::run()
{
  assert(is_head());
  struct timeval now;
  click_gettimeofday(&now);
  while (_next != this && !timercmp(&_next->_expires, &now, >)) {
    Timer *t = _next;
    _next = t->_next;
    _next->_prev = this;
    t->_prev = 0;
    t->_hook(t, t->_thunk);
  }
}

// How long select() should wait until next timer expires.
int
Timer::get_next_delay(struct timeval *tv)
{
  assert(is_head());
  if (_next == this) {
    tv->tv_sec = 1000;
    tv->tv_usec = 0;
    return 0;
  } else {
    struct timeval now;
    click_gettimeofday(&now);
    if(timercmp(&_next->_expires, &now, >)){
      timersub(&_next->_expires, &now, tv);
    } else {
      tv->tv_sec = 0;
      tv->tv_usec = 0;
    }
    return 1;
  }
}


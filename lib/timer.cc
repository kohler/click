/*
 * timer.{cc,hh} -- portable timers. Linux kernel module uses Linux timers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "timer.hh"
#include "element.hh"
#include "router.hh"

/*
 * Timer::element_timer is a callback that gets called when a Timer,
 * constructed with just an Element instance, expires. 
 * 
 * When this is used in kernel interrupt mode, this callback gets called in
 * the timer_bh of the kernel. Since linux kernel bottom halves are not
 * reentrant (i.e. only ONE bottom half handler can be executing at a time -
 * see kernel/softirq.c), this will not cause any race conditions with
 * router->driver() in FromDevice.
 *
 * When used in userlevel or kernel polling mode, timer is maintained by
 * Click, so Timer::element_timer is called within click.
 */
void
Timer::element_timer(unsigned long thunk)
{
  Element *f = (Element *)thunk;
  /* put itself on the work list */
  f->join_scheduler();
#ifdef __KERNEL__
#ifndef HAVE_POLLING
  /* run work list */
  f->router()->driver();
#endif
#endif
}

#if !defined(__KERNEL__) || defined(HAVE_POLLING)

static Timer timer_head(0, (unsigned long)0);

void
Timer::static_initialize()
{
  timer_head._prev = timer_head._next = 0;
}

void
Timer::schedule_after_ms(int ms)
{
  if (scheduled())
    unschedule();
  click_gettimeofday(&_expires);
  struct timeval interval;
  interval.tv_sec = ms / 1000;
  interval.tv_usec = (ms % 1000) * 1000;
  timeradd(&_expires, &interval, &_expires);
  Timer *prev = &timer_head;
  Timer *trav;
  for (trav = prev->_next;
       trav && timercmp(&_expires, &trav->_expires, >);
       prev = trav, trav = trav->_next)
    /* nada */;
  _prev = prev;
  _next = trav;
  _prev->_next = this;
  if (trav) trav->_prev = this;
}

void
Timer::unschedule()
{
  if (scheduled()) {
    _prev->_next = _next;
    if (_next) _next->_prev = _prev;
    _prev = _next = 0;
  }
}

void
Timer::run_timers()
{
  struct timeval now;
  click_gettimeofday(&now);
  Timer *next;
  while ((next = timer_head._next) && !timercmp(&next->_expires, &now, >)) {
    timer_head._next = next->_next;
    if (next->_next) next->_next->_prev = &timer_head;
    next->_next = next->_prev = 0;
    next->_hook(next->_thunk);
  }
}

// How long select() should wait until next timer expires.
int
Timer::get_next_delay(struct timeval *tv)
{
  Timer *next;
  if((next = timer_head._next) == 0){
    tv->tv_sec = 1000;
    tv->tv_usec = 0;
    return(0);
  } else {
    struct timeval now;
    click_gettimeofday(&now);
    if(timercmp(&next->_expires, &now, >)){
      timersub(&next->_expires, &now, tv);
    } else {
      tv->tv_sec = 0;
      tv->tv_usec = 0;
    }
    return(1);
  }  
}

#endif /* !__KERNEL__ || HAVE_POLLING */


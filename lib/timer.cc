#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "timer.hh"
#include "element.hh"
#include "router.hh"

void
Timer::element_timer(unsigned long thunk)
{
  Element *f = (Element *)thunk;
  f->schedule_tail(); // don't do anything - just put it on the work list
#ifdef __KERNEL__
  // run work list
  f->router()->run_scheduled();
#endif
}

#ifndef __KERNEL__

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

#endif /* !__KERNEL__ */

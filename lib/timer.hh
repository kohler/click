#ifndef TIMER_HH
#define TIMER_HH
#include "glue.hh"
class Element;

typedef void (*TimerHook)(unsigned long);

#ifdef __KERNEL__
#include <linux/timer.h>

class Timer {
  
  struct timer_list _t;

  Timer(const Timer &);
  Timer &operator=(const Timer &);

  static void element_timer(unsigned long);
  
 public:
  
  Timer(TimerHook, unsigned long);
  Timer(Element *);
  ~Timer()				{ unschedule(); }
  
  bool scheduled() const		{ return timer_pending(&_t); }
  
  void schedule_after_ms(int);
  void unschedule()			{ if (scheduled()) del_timer(&_t); }
  
};

inline
Timer::Timer(TimerHook hook, unsigned long thunk)
{
  init_timer(&_t);
  _t.function = hook;
  _t.data = thunk;
}

inline
Timer::Timer(Element *f)
{
  init_timer(&_t);
  _t.function = element_timer;
  _t.data = (unsigned long)f;
}

inline void
Timer::schedule_after_ms(int ms)
{
#if CLICK_HZ == 100
  unsigned long time = jiffies + (ms / 10);
#else
  unsigned long time = jiffies + (ms * CLICK_HZ / 1000);
#endif
  if (scheduled())
    mod_timer(&_t, time);
  else {
    _t.expires = time;
    add_timer(&_t);
  }
}

#else /* !__KERNEL__ */

class Timer {
  
  Timer *_prev;
  Timer *_next;
  struct timeval _expires;
  TimerHook _hook;
  unsigned long _thunk;

  Timer(const Timer &);
  Timer &operator=(const Timer &);

  static void element_timer(unsigned long);
  
 public:
  
  Timer(TimerHook, unsigned long);
  Timer(Element *);
  ~Timer()				{ if (scheduled()) unschedule(); }
  
  bool scheduled() const		{ return _prev != 0; }
  
  void schedule_after_ms(int);
  void unschedule();

  static void static_initialize();
  static void run_timers();
  static int get_next_delay(struct timeval *tv);
  
};

inline
Timer::Timer(TimerHook hook, unsigned long thunk)
  : _prev(0), _next(0), _hook(hook), _thunk(thunk)
{
}

inline
Timer::Timer(Element *f)
  : _prev(0), _next(0), _hook(element_timer), _thunk((unsigned long)f)
{
}

#endif /* __KERNEL__ */

#endif

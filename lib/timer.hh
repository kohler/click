#ifndef TIMER_HH
#define TIMER_HH
#include "glue.hh"
#include <assert.h>
class Element;
class Router;

typedef void (*TimerHook)(unsigned long);

class Timer {
  
  Timer *_prev;
  Timer *_next;
  struct timeval _expires;
  TimerHook _hook;
  unsigned long _thunk;
  Timer *_head;

  Timer(const Timer &);
  Timer &operator=(const Timer &);

  static void element_hook(unsigned long);
  static void head_hook(unsigned long);
  
 public:
  
  Timer();				// create head
  Timer(Timer *, TimerHook, unsigned long);
  Timer(TimerHook, unsigned long);
  Timer(Element *);
  ~Timer()				{ if (scheduled()) unschedule(); }

  // functions on Timers
  bool attached() const			{ return _head != 0; }
  bool scheduled() const		{ return _prev != 0; }
  void attach(Timer *);
  void attach(Router *);
  void attach(Element *);
  void schedule_after_ms(int);
  void unschedule();

  // functions on heads
  bool is_head() const			{ return _hook == head_hook; }
  void run();
  int get_next_delay(struct timeval *tv);
  
};

inline void
Timer::attach(Timer *t)
{
  assert(!attached() && t->is_head());
  _head = t;
}

#endif

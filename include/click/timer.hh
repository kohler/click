#ifndef TIMER_HH
#define TIMER_HH
#include <click/glue.hh>
#include <assert.h>
class Element;
class Router;
class Timer;

typedef void (*TimerHook)(Timer *, void *);

class Timer { public:

  Timer();				// create head
  Timer(Timer *, TimerHook, void *);
  Timer(TimerHook, void *);
  Timer(Element *);
  ~Timer()				{ if (scheduled()) unschedule(); }

  // functions on Timers
  bool attached() const			{ return _head != 0; }
  bool scheduled() const		{ return _prev != 0; }
  void attach(Timer *);
  void attach(Router *);
  void attach(Element *);
  void schedule_at(const struct timeval &);
  void schedule_after_ms(int);
  void unschedule();

  // functions on heads
  bool is_head() const			{ return _hook == head_hook; }
  void run();
  int get_next_delay(struct timeval *tv);
  
 private:
  
  Timer *_prev;
  Timer *_next;
  struct timeval _expires;
  TimerHook _hook;
  void *_thunk;
  Timer *_head;

  Timer(const Timer &);
  Timer &operator=(const Timer &);

  void finish_schedule();
  
  static void element_hook(Timer *, void *);
  static void head_hook(Timer *, void *);
  
};

inline void
Timer::attach(Timer *t)
{
  assert(!attached() && t->is_head());
  _head = t;
}

#endif

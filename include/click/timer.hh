#ifndef TIMER_HH
#define TIMER_HH
#include <click/glue.hh>
#include <assert.h>
class Element;
class Router;
class Timer;
class Task;

typedef void (*TimerHook)(Timer *, void *);

class Timer { public:

  Timer();				// create head
  Timer(Timer *, TimerHook, void *);
  Timer(TimerHook, void *);
  Timer(Element *);
  Timer(Task *); 
  ~Timer()				{ if (scheduled()) unschedule(); }

  // functions on Timers
  bool initialized() const		{ return _head != 0; }
  bool scheduled() const		{ return _prev != 0; }
  void initialize(Timer *);
  void initialize(Router *);
  void initialize(Element *);
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
  static void task_hook(Timer *, void *);
  static void head_hook(Timer *, void *);
  
};

inline void
Timer::initialize(Timer *t)
{
  assert(!initialized() && t->is_head());
  _head = t;
}

#endif

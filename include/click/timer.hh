#ifndef TIMER_HH
#define TIMER_HH
#include <click/sync.hh>
#include <click/glue.hh>
#include <assert.h>
class Element;
class Router;
class Timer;
class TimerList;
class Task;

typedef void (*TimerHook)(Timer *, void *);

class Timer { public:

  Timer(TimerHook, void *);
  Timer(Element *);			// call element->run_scheduled()
  Timer(Task *);			// call task->reschedule()
  ~Timer()				{ if (scheduled()) unschedule(); }

  // functions on Timers
  bool initialized() const		{ return _head != 0; }
  bool scheduled() const		{ return _prev != 0; }
  bool is_list() const;
  
  void initialize(TimerList *);
  void initialize(Router *);
  void initialize(Element *);
  void uninitialize();			// equivalent to unschedule()

  void schedule_now();
  void schedule_at(const struct timeval &);
  void schedule_after_ms(int);
  void unschedule();

 private:
  
  Timer *_prev;
  Timer *_next;
  struct timeval _expires;
  TimerHook _hook;
  void *_thunk;
  TimerList *_head;

  Timer(const Timer &);
  Timer &operator=(const Timer &);

  void finish_schedule();
  
  static void element_hook(Timer *, void *);
  static void task_hook(Timer *, void *);

  friend class TimerList;
  
};

class TimerList : public Timer { public:

  TimerList();

  void run();
  int get_next_delay(struct timeval *tv);
  
 private:

  Spinlock _lock;
  
  static void list_hook(Timer *, void *);

  friend class Timer;
  
};

inline void
Timer::initialize(TimerList *t)
{
  assert(!initialized());
  _head = t;
}

inline void
Timer::uninitialize()
{
  unschedule();
}

inline bool
Timer::is_list() const
{
  return _head == this;
}

inline void
Timer::schedule_now()
{
  schedule_after_ms(0);
}

#endif

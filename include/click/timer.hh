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

  bool initialized() const		{ return _head != 0; }
  bool scheduled() const		{ return _prev != 0; }
  bool is_list() const;
  
  void initialize(TimerList *);
  void initialize(Router *);
  void initialize(Element *);
  void uninitialize()			{ unschedule(); }

  void schedule_now();
  void schedule_at(const struct timeval &);
  void schedule_after_s(uint32_t);
  void schedule_after_ms(uint32_t);
  void reschedule_at(const struct timeval &);
  void reschedule_after_s(uint32_t);
  void reschedule_after_ms(uint32_t);
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

  friend class TimerList;
  
};

class TimerList : public Timer { public:

  TimerList();

  void run();
  int get_next_delay(struct timeval *tv);

  void unschedule_all();
  
 private:

  Spinlock _lock;
  
  void acquire_lock()			{ _lock.acquire(); }
  bool attempt_lock()			{ return _lock.attempt(); }
  void release_lock()			{ _lock.release(); }

  friend class Timer;
  
};

inline void
Timer::initialize(TimerList *t)
{
  assert(!initialized());
  _head = t;
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

inline void
Timer::reschedule_at(const struct timeval &tv)
{
  schedule_at(tv);
}

#endif

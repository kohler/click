#ifndef CLICK_TASK_HH
#define CLICK_TASK_HH
#include <click/element.hh>
#if __MTCLICK__
# include <click/ewma.hh>
#endif

#define PASS_GT(a, b)	((int)(a - b) > 0)

typedef void (*TaskHook)(void *);
class TaskList;

class Task { public:

  static const unsigned STRIDE1 = 1U<<16;
  static const unsigned MAX_STRIDE = 1U<<31;
  static const int MAX_TICKETS = 1U<<15;
  
  Task();
  Task(TaskHook, void *);
  Task(Element *);

  bool initialized() const		{ return _all_prev; }
  bool scheduled() const		{ return _prev; }
  
  Task *scheduled_next() const		{ return _next; }
  Task *scheduled_prev() const		{ return _prev; }
  TaskList *scheduled_list() const	{ return _list; }

  bool is_list() const;

#ifndef RR_SCHED
  int tickets() const			{ return _tickets; }
  int max_tickets() const		{ return _max_tickets; }
  
  void set_max_tickets(int);
  void set_tickets(int);
  void adj_tickets(int);
#endif

  bool urgent() const			{ return _urgent; }
  void set_urgent(bool v)		{ _urgent = v; }

  void initialize(TaskList *);
  void initialize(Element *);
  void initialize(Router *);
  void uninitialize();

  void join_scheduler(TaskList *);
  void join_scheduler(Element *);
  void join_scheduler(Router *);
  
  void reschedule();
  void unschedule();
  void schedule_immediately();

  void fast_reschedule();
  void fast_unschedule();

  void call_hook();

#if __MTCLICK__
  int cycles() const;
  void update_cycles(unsigned c);
  int thread_preference() const		{ return _thread_preference; }
  void set_thread_preference(int s)	{ _thread_preference = s; }
#endif

  Task *initialized_prev() const	{ return _all_prev; }
  Task *initialized_next() const	{ return _all_next; }

 private:

  /* if gcc keeps this ordering, we may get some cache locality on a 16 or 32
   * byte cache line: the first three fields are used in list traversal */

  Task *_prev;
  Task *_next;
#ifndef RR_SCHED
  unsigned _pass;
  unsigned _stride;
  int _tickets;
  int _max_tickets;
#endif

  TaskHook _hook;
  void *_thunk;
  bool _urgent;
  
#if __MTCLICK__
  DirectEWMA _cycles;
  int _thread_preference;
#endif

  TaskList *_list;

  Task *_all_prev;
  Task *_all_next;

  friend class TaskList;

};

class TaskList : public Task { public:

  TaskList();

  bool empty() const;

  void lock()				{ }
  void unlock()				{ }
  
 private:

  friend class Task;
  
};


inline
Task::Task()
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(0), _thunk(0), _urgent(false),
#if __MTCLICK__
    _thread_preference(-1),
#endif
    _list(0), _all_prev(0), _all_next(0)
{
}

inline
Task::Task(TaskHook hook, void *thunk)
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(hook), _thunk(thunk), _urgent(false),
#if __MTCLICK__
    _thread_preference(-1),
#endif
    _list(0), _all_prev(0), _all_next(0)
{
}

inline
Task::Task(Element *e)
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(0), _thunk(e), _urgent(false),
#if __MTCLICK__
    _thread_preference(-1),
#endif
    _list(0), _all_prev(0), _all_next(0)
{
}

inline bool
TaskList::empty() const
{
  return _next == const_cast<Task*>(reinterpret_cast<const Task*>(this));
}

inline bool
Task::is_list() const
{
  return _list == this;
}

inline void
Task::fast_unschedule()
{
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }
  _next = _prev = 0;
}

#ifndef RR_SCHED

inline void 
Task::set_tickets(int n)
{
  if (n > _max_tickets)
    n = _max_tickets;
  else if (n < 1)
    n = 1;
  _tickets = n;
  _stride = STRIDE1 / n;
  assert(_stride < MAX_STRIDE);
}

inline void 
Task::adj_tickets(int delta)
{
  set_tickets(_tickets + delta);
}

inline void
Task::fast_reschedule()
{
  // should not be scheduled at this point
  assert(!_next);
#if 0
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }
#endif

  // increase pass
  _pass += _stride;

#if 0
  // look for element before where we should be scheduled
  Task *n = _list->_prev;
  while (n != _list && !PASS_GT(_pass, n->_pass))
    n = n->_prev;

  // schedule after `n'
  _next = n->_next;
  _prev = n;
  n->_next = this;
  _next->_prev = this;
#else
  // look for element after where we should be scheduled
  Task *n = _list->_next;
  while (n != _list && !PASS_GT(n->_pass, _pass))
    n = n->_next;

  // schedule before `n'
  _prev = n->_prev;
  _next = n;
  _prev->_next = this;
  n->_prev = this;
#endif
}

#else /* RR_SCHED */

inline void
Task::fast_reschedule()
{
  _prev = _list->_prev;
  _next = _list;
  _list->_prev = this;
  _prev->_next = this;
}

#endif /* RR_SCHED */

inline void
Task::join_scheduler(TaskList *task_list)
{
  assert(initialized() && !_prev && !_next && task_list->is_list());
  _list = task_list;
  reschedule();
}

inline void
Task::join_scheduler(Element *e)
{
  join_scheduler(e->router());
}

inline void
Task::call_hook()
{
  if (!_hook)
    ((Element *)_thunk)->run_scheduled();
  else
    _hook(_thunk);
}

#if __MTCLICK__
inline int
Task::cycles() const
{
  return _cycles.average() >> _cycles.scale;
}

inline void
Task::update_cycles(unsigned c) 
{
  _cycles.update_with(c);
}
#endif

#endif

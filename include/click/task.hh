#ifndef CLICK_TASK_HH
#define CLICK_TASK_HH
#include <click/glue.hh>
#include <click/element.hh>

#define PASS_GT(a, b)	((int)(a - b) > 0)

typedef void (*TaskHook)(void *);

class Task { public:

  static const unsigned STRIDE1 = 1U<<16;
  static const unsigned MAX_STRIDE = 1U<<31;
  static const int MAX_TICKETS = 1U<<15;
  
  Task();
  Task(TaskHook, void *);
  Task(Element *);

  bool attached() const			{ return _list; }
  bool scheduled() const		{ return _prev; }
  Task *next_task() const		{ return _next; }
  Task *prev_task() const		{ return _prev; }
  Task *task_list() const		{ return _list; }

  bool empty() const			{ return _next == this; }
  
  void attach(Task *);
  void attach(Element *);
  void attach(Router *);
  void initialize_list()		{ _prev = _next = _list = this; }

#ifndef RR_SCHED
  int tickets() const			{ return _tickets; }
  int max_tickets() const		{ return _max_tickets; }
  
  void set_max_tickets(int);
  void set_tickets(int);
  void adj_tickets(int);
#endif
  
  void join_scheduler();
  void join_scheduler(Task *);		// like attach(...); join_scheduler();
  void join_scheduler(Element *);
  void join_scheduler(Router *);
  
  void schedule_immediately();
  void unschedule();
  void reschedule();

  void call_hook();

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
  
  Task *_list;

};


inline
Task::Task()
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(0), _thunk(0), _list(0)
{
}

inline
Task::Task(TaskHook hook, void *thunk)
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(hook), _thunk(thunk), _list(0)
{
}

inline
Task::Task(Element *e)
  : _prev(0), _next(0),
#ifndef RR_SCHED
    _pass(0), _stride(0), _tickets(-1), _max_tickets(-1),
#endif
    _hook(0), _thunk(e), _list(0)
{
}

inline void
Task::attach(Task *r)
{
  assert(!_prev && !_next);
  _list = r;
}

inline void
Task::unschedule()
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
Task::reschedule()
{
  // should not be scheduled at this point
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }

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

inline void
Task::join_scheduler()
{
  assert(_list);
  if (_tickets < 1 || scheduled())
    return;
  if (_list->_next == _list)
    /* nothing on worklist */
    _pass = 0;
  else 
    _pass = _list->_next->_pass;
  reschedule();
}

#else /* RR_SCHED */

inline void
Task::reschedule()
{
  _prev = _list->_prev;
  _next = _list;
  _list->_prev = this;
  _prev->_next = this;
}

inline void
Task::join_scheduler()
{
  if (!scheduled())
    reschedule();
}

#endif /* RR_SCHED */

inline void
Task::join_scheduler(Task *t)
{
  attach(t);
  join_scheduler();
}

inline void
Task::join_scheduler(Element *e)
{
  attach(e);
  join_scheduler();
}

inline void
Task::join_scheduler(Router *r)
{
  attach(r);
  join_scheduler();
}

inline void
Task::schedule_immediately()
{
  // should not be scheduled at this point
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }

  _next = _list->_next;
  _prev = _list;
  _list->_next = this;
  _next->_prev = this;
}

inline void
Task::call_hook()
{
  if (!_hook)
    ((Element *)_thunk)->run_scheduled();
  else
    _hook(_thunk);
}

#endif

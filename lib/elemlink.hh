#ifndef ELEMLINK_HH
#define ELEMLINK_HH
#include "glue.hh"
#include "mplock.hh"
#include <assert.h>

#define PASS_GT(a, b)	((int)(a - b) > 0)

class ElementLink {

  /* if gcc keeps this ordering, we may get some cache locality on a 16 or 32
   * byte cache line: the first three fields are used in list traversal */

  ElementLink *_prev;
  ElementLink *_next;
#ifndef RR_SCHED
  unsigned _pass;
  unsigned _stride;
  int _tickets;
  int _max_tickets;
#endif
  ElementLink *_list;
  Spinlock _worklist_lock;

 public:

  static const unsigned STRIDE1 = 1U<<16;
  static const unsigned MAX_STRIDE = 1U<<31;
  static const int MAX_TICKETS = 1U<<15;
  
  ElementLink()				
    : _prev(0), _next(0)
#ifndef RR_SCHED
    , _pass(0), _stride(0), _tickets(-1), _max_tickets(-1)
#endif
    { }

  bool worklist_lock_attempt();
  void worklist_lock_acquire();
  void worklist_lock_release();

  bool scheduled() const		{ return _prev; }
  ElementLink *scheduled_next() const	{ return _next; }
  ElementLink *scheduled_prev() const	{ return _prev; }
  ElementLink *scheduled_list() const	{ return _list; }
  
  void initialize_link(ElementLink *);
  void initialize_head()		{ _prev = _next = _list = this; }

#ifndef RR_SCHED
  int tickets() const			{ return _tickets; }
  int max_tickets() const		{ return _max_tickets; }
  
  void set_max_tickets(int);
  void set_tickets(int);
  void adj_tickets(int);
#endif
  
  void join_scheduler();
  void schedule_immediately();
  void unschedule();
  void reschedule();

};


inline bool
ElementLink::worklist_lock_attempt()
{
  if (_list) 
    return _list->_worklist_lock.attempt();
  else
    return true;
}

inline void
ElementLink::worklist_lock_acquire()
{
  if (_list)
    _list->_worklist_lock.acquire();
}

inline void
ElementLink::worklist_lock_release()
{
  if (_list) 
    _list->_worklist_lock.release();
}

inline void
ElementLink::initialize_link(ElementLink *r)
{
  assert(!_prev && !_next);
  _list = r;
}

inline void
ElementLink::unschedule()
{
  worklist_lock_acquire();
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }
  _next = _prev = 0;
  worklist_lock_release();
}

#ifndef RR_SCHED

inline void 
ElementLink::set_tickets(int n)
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
ElementLink::adj_tickets(int delta)
{
  set_tickets(_tickets + delta);
}

inline void
ElementLink::reschedule()
{
  worklist_lock_acquire();

  // should not be scheduled at this point
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }

  // increase pass
  _pass += _stride;

#if 0
  // look for element before where we should be scheduled
  ElementLink *n = _list->_prev;
  while (n != _list && !PASS_GT(_pass, n->_pass))
    n = n->_prev;

  // schedule after `n'
  _next = n->_next;
  _prev = n;
  n->_next = this;
  _next->_prev = this;
#else
  // look for element after where we should be scheduled
  ElementLink *n = _list->_next;
  while (n != _list && !PASS_GT(n->_pass, _pass))
    n = n->_next;

  // schedule before `n'
  _prev = n->_prev;
  _next = n;
  _prev->_next = this;
  n->_prev = this;
#endif
 
  worklist_lock_release();
}

inline void
ElementLink::join_scheduler()
{
  worklist_lock_acquire();
  if (_tickets < 1 || scheduled()) {
    worklist_lock_release();
    return;
  }
  if (_list->_next == _list)
    /* nothing on worklist */
    _pass = 0;
  else 
    _pass = _list->_next->_pass;
  reschedule();
 
  worklist_lock_release();
}

#else /* RR_SCHED */

inline void
ElementLink::reschedule()
{
  worklist_lock_acquire();
  ElementLink *n = _list->_next;
  _prev = _list->_prev;
  _next = _list;
  _list->_prev = this;
  _prev->_next = this;
  worklist_lock_release();
}

inline void
ElementLink::join_scheduler()
{
  worklist_lock_acquire();
  if (!scheduled())
    reschedule();
  worklist_lock_release();
}

#endif /* RR_SCHED */

inline void
ElementLink::schedule_immediately()
{
  worklist_lock_acquire();
  // should not be scheduled at this point
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }

  _next = _list->_next;
  _prev = _list;
  _list->_next = this;
  _next->_prev = this;
  
  worklist_lock_release();
}

#endif

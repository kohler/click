#ifndef ELEMLINK_HH
#define ELEMLINK_HH
#include "glue.hh"
#include <assert.h>


#define PASS_GT(a, b)	((int)(a - b) > 0)

#ifndef RR_SCHED

class ElementLink {

  /* if gcc keeps this ordering, we may get some cache locality on a 16 or 32
   * byte cache line: the first three fields are used in list traversal */

  ElementLink *_prev;
  ElementLink *_next;
  unsigned _pass;
  unsigned _stride;
  int _tickets;
  int _max_tickets;
  ElementLink *_list;

 public:


  static const unsigned STRIDE1 = 1U<<16;
  static const unsigned MAX_STRIDE = 1U<<31;
  static const int MAX_TICKETS = 1U<<15;
  
  ElementLink()				
    : _prev(0), _next(0), _pass(0), 
      _stride(0), _tickets(-1), _max_tickets(-1) { }

  bool scheduled() const		{ return _prev; }
  ElementLink *scheduled_next() const	{ return _next; }
  ElementLink *scheduled_prev() const	{ return _prev; }
  ElementLink *scheduled_list() const	{ return _list; }
  
  void initialize_link(ElementLink *);
  void initialize_head()		{ _prev = _next = _list = this; }
  
  int tickets() const			{ return _tickets; }
  int max_tickets() const		{ return _max_tickets; }
  
  void set_max_tickets(int);
  void set_tickets(int);
  void adj_tickets(int);
  
  void join_scheduler();
  void unschedule();
  void reschedule();

};


inline void
ElementLink::initialize_link(ElementLink *r)
{
  assert(!_prev && !_next);
  _list = r;
}

inline void
ElementLink::unschedule()
{
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }
  _next = _prev = 0;
}

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
}

inline void
ElementLink::join_scheduler()
{
  if (_tickets < 1 || scheduled()) return;
  if (_list->_next == _list)
    /* nothing on worklist */
    _pass = 0;
  else 
    _pass = _list->_next->_pass;
  reschedule();
}

#else

class Router;
class ElementLink {
  bool _scheduled;

protected:
  Router *_router;

public:

  ElementLink()	: _scheduled(false) { }

  bool scheduled() const		{ return _scheduled; }
  void initialize_link(Router *r) 	{ _router = r; }

  void join_scheduler() 		{ _scheduled = true; }
  void reschedule()			{ }
  void unschedule() 			{ }
};

#endif

#endif


#ifndef ELEMLINK_HH
#define ELEMLINK_HH

#include "glue.hh"
#include <assert.h>

#define STRIDE1     100000
#define MAX_PASS    (1LL<<61)

class ElementLink {

  /* if gcc keeps this ordering, we may get some cache locality on a 16 or 32
   * byte cache line: the first three fields are used in list traversal */

  ElementLink *_prev;
  ElementLink *_next;
  long long _pass;

  unsigned int _stride;
  long long _remain;
  int _ntickets;

  ElementLink *_list;

  static unsigned int _global_tickets;
  static unsigned int _global_stride;
  static unsigned long long _global_elapsed;
  static unsigned long long _global_pass;
  
  static void global_pass_update();
  static void global_tickets_update(int delta);
  
 public:
  
  ElementLink()				
    : _prev(0), _next(0), _pass(0), _stride(0), _remain(0), _ntickets(-1) { }
  
  ElementLink(ElementLink *lst)
    : _prev(0), _next(0), _pass(0), _stride(0), _remain(0), _ntickets(-1), 
      _list(lst) { }

  bool scheduled() const		{ return _prev; }
  ElementLink *scheduled_next() const	{ return _next; }
  ElementLink *scheduled_prev() const	{ return _prev; }
  ElementLink *scheduled_list() const	{ return _list; }
  
  void initialize_link(ElementLink *);
  void initialize_head()		{ _prev = _next = _list = this; }
  
  void unschedule();
  void schedule_before(ElementLink *);
  void schedule_head();
  void schedule_tail();

  int ntickets() const			{ return _ntickets; }
  void set_ntickets(int);
  void join_scheduler();
  void leave_scheduler();
  void reschedule();
  
  void stride() { _pass += _stride; }
  void refresh_worklist_passes();
  
  static void elapse() { _global_elapsed++; }
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
ElementLink::schedule_head()
{
  if (_next) {
    _next->_prev = _prev;
    _prev->_next = _next;
  }
  ElementLink *n = _list->_next;
  _next = n;
  _prev = _list;
  _list->_next = this;
  n->_prev = this;
}

inline void
ElementLink::schedule_tail()
{
  if (!_next) {
    ElementLink *p = _list->_prev;
    _next = _list;
    _prev = p;
    _list->_prev = this;
    p->_next = this;
  }
}

inline void 
ElementLink::schedule_before(ElementLink *n)
{
  if (n) {
    unschedule();
    _next = n;
    _prev = n->_prev;
    n->_prev = this;
    _prev->_next = this;
  }
}

inline void
ElementLink::reschedule()
{
  unschedule();
  ElementLink *n = scheduled_list()->scheduled_next();

  while(n != scheduled_list()) {
    if (n->_pass > _pass) {
      schedule_before(n);
      return;
    }
    n = n->scheduled_next();
  }

  // largest pass value, schedule_tail so we don't reschedule immediately
  schedule_tail();
}

inline void
ElementLink::join_scheduler()
{
  if (_ntickets < 1 || scheduled()) return;
    
  global_pass_update();
  _pass = _global_pass + _remain;
  global_tickets_update(_ntickets);
  
  reschedule();
}

inline void
ElementLink::leave_scheduler()
{
  if (_ntickets < 1) return;
  
  global_pass_update();
  _remain = _pass - _global_pass;
  global_tickets_update(0-_ntickets);
  
  unschedule();
}

inline void
ElementLink::set_ntickets(int n)
{
  _ntickets = n;
  if (n > 0) {
    _stride = STRIDE1 / n;
    _remain = _stride;
  }
}



inline void 
ElementLink::global_pass_update()
{
  _global_pass += _global_stride * _global_elapsed;
  _global_elapsed = 0;
}

inline void 
ElementLink::global_tickets_update(int delta)
{
  _global_tickets += delta;
  if (_global_tickets > 0) 
    _global_stride = STRIDE1 / _global_tickets;
  else 
    _global_stride = 0;
}
  
inline void 
ElementLink::refresh_worklist_passes()
{
  int base = -1;
  ElementLink *n = scheduled_list()->scheduled_next();

  click_chatter("redoing worklist");

  while(n != scheduled_list()) {
    if (base == -1) base = n->_pass;
    n->_pass -= base;
    if (n->_pass > MAX_PASS)
      click_chatter("warning: element has high pass value");
    n = n->scheduled_next();
  }

  _global_pass = 0;
  _global_elapsed = 0;
}

#endif


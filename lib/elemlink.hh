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
  int _ntickets;
  int _max_ntickets;
  ElementLink *_list;
  
  void stride() { _pass += _stride; }
  void schedule_before(ElementLink *);
  void schedule_tail();

 public:
  
  ElementLink()				
    : _prev(0), _next(0), _pass(0), 
      _stride(0), _ntickets(-1), _max_ntickets(-1) { }
  
  ElementLink(ElementLink *lst)
    : _prev(0), _next(0), _pass(0), 
      _stride(0), _ntickets(-1), _max_ntickets(-1), _list(lst) { }

  bool scheduled() const		{ return _prev; }
  ElementLink *scheduled_next() const	{ return _next; }
  ElementLink *scheduled_prev() const	{ return _prev; }
  ElementLink *scheduled_list() const	{ return _list; }
  
  void initialize_link(ElementLink *);
  void initialize_head()		{ _prev = _next = _list = this; }
  
  int ntickets() const			{ return _ntickets; }
  int max_ntickets() const		{ return _max_ntickets; }
  void set_max_ntickets(int);
  void join_scheduler();
  void reschedule();
  void adj_tickets(int);
  void unschedule();

  void refresh_worklist_passes();
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
ElementLink::adj_tickets(int n)
{
  _ntickets += n;
  if (_ntickets > _max_ntickets)
    _ntickets = _max_ntickets;
  else if (_ntickets < 1)
    _ntickets = 1;
  _stride = STRIDE1 / _ntickets;
}

inline void
ElementLink::reschedule()
{
  stride();
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
  if (_pass > MAX_PASS) refresh_worklist_passes();
}

inline void
ElementLink::join_scheduler()
{
  if (_ntickets < 1 || scheduled()) return;
  if (scheduled_list()->scheduled_next() == scheduled_list())
    /* nothing on worklist */
    _pass = 0;
  else 
    _pass = scheduled_list()->scheduled_next()->_pass;
  reschedule();
}

inline void
ElementLink::set_max_ntickets(int n)
{
  _max_ntickets = n;
  if (n > 0) {
    _ntickets = 1;
    _stride = STRIDE1 / _ntickets;
  } else {
    _ntickets = -1;
    _stride = 0;
  }
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
}

#endif


#ifndef ELEMLINK_HH
#define ELEMLINK_HH

class ElementLink {
  
  ElementLink *_prev;
  ElementLink *_next;
  ElementLink *_list;
  
 public:
  
  ElementLink()				: _prev(0), _next(0) { }
  ElementLink(ElementLink *lst)		: _prev(0), _next(0), _list(lst) { }
  
  bool scheduled() const		{ return _prev; }
  ElementLink *scheduled_next() const	{ return _next; }
  ElementLink *scheduled_prev() const	{ return _prev; }
  ElementLink *scheduled_list() const	{ return _list; }
  
  void initialize_link(ElementLink *);
  void initialize_head()		{ _prev = _next = _list = this; }
  
  void unschedule();
  void schedule_head();
  void schedule_tail();
  
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

#endif

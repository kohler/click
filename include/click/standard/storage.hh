#ifndef CLICK_STORAGE_HH
#define CLICK_STORAGE_HH

class Storage { public:

  Storage()				: _head(0), _tail(0) { }

  operator bool() const			{ return _head != _tail; }
  bool empty() const			{ return _head == _tail; }
  int size() const;
  int capacity() const			{ return _capacity; }

  int next_i(int i) const		{ return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const		{ return (i!=0 ? i-1 : _capacity); }

  // to be used with care...
  void set_capacity(int c)		{ _capacity = c; }
  void set_head(int h)			{ _head = h; }
  void set_tail(int t)			{ _tail = t; }
  
 protected:

  int _capacity;
  int _head;
  int _tail;
  
};

inline int
Storage::size() const
{
  register int x = _tail - _head;
  return (x >= 0 ? x : _capacity + x + 1);
}

#endif

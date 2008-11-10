// -*- c-basic-offset: 4 -*-
#ifndef CLICK_STORAGE_HH
#define CLICK_STORAGE_HH
CLICK_DECLS

class Storage { public:

    Storage()				: _head(0), _tail(0) { }

    operator bool() const		{ return _head != _tail; }
    bool empty() const			{ return _head == _tail; }
    int size() const;
    int size(int head, int tail) const;
    int capacity() const		{ return _capacity; }

    int head() const			{ return _head; }
    int tail() const			{ return _tail; }

    int next_i(int i) const		{ return (i!=_capacity ? i+1 : 0); }
    int prev_i(int i) const		{ return (i!=0 ? i-1 : _capacity); }

    // to be used with care
    void set_capacity(int c)		{ _capacity = c; }
    void set_head(int h)		{ _head = h; }
    void set_tail(int t)		{ _tail = t; }

  protected:

    int _capacity;
    volatile int _head;
    volatile int _tail;

};

inline int
Storage::size(int head, int tail) const
{
    int x = tail - head;
    return (x >= 0 ? x : _capacity + x + 1);
}

inline int
Storage::size() const
{
    return size(_head, _tail);
}

CLICK_ENDDECLS
#endif

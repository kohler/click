/*
 * vector.{cc,hh} -- simple array template class
 * Douglas S. J. De Couto
 * Based on code from Click Vector<> class.
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#ifndef CLICK_VECTOR_CC
#define CLICK_VECTOR_CC

CLICK_ENDDECLS
#include <click/dequeue.hh>
CLICK_DECLS

template <class T>
DEQueue<T>::DEQueue(const DEQueue<T> &o)
  : _l(0), _n(0), _cap(0), _head(0), _tail(0)
{
  *this = o;
}

template <class T>
DEQueue<T>::~DEQueue()
{
  for (int i = _head, j = 0; j < _n; i = next_i(i), j++)
    _l[i].~T();
  delete[] (unsigned char *)_l;
}

template <class T> DEQueue<T> &
DEQueue<T>::operator=(const DEQueue<T> &o)
{
  if (&o != this) {
    for (int i = _head, j = 0; j < _n; i = next_i(i), j++)
      _l[i].~T();
    _n = 0;
    _head = 0;
    _tail = 0;
    if (reserve(o._n)) {
      _n = o._n;
      for (int i = 0, j = o._head; i < _n; i++, j = o.next_i(j))
        new(velt(i)) T(o._l[j]);
      _tail = _n;
    }
  }
  return *this;
}

template <class T> DEQueue<T> &
DEQueue<T>::assign(int n, const T &e)
{
  resize(0, e);
  resize(n, e);
  return *this;
}

template <class T> bool
DEQueue<T>::reserve(int want)
{
  if (want < 0)
    want = _cap > 0 ? _cap * 2 : 4;
  if (want <= _cap)
    return true;
  
  T *new_l = (T *)new unsigned char[sizeof(T) * want];
  if (!new_l) return false;
  
  for (int i = _head, j = 0; j < _n; j++, i = next_i(i)) {
    new(velt(new_l, j)) T(_l[i]);
    _l[i].~T();
  }
  delete[] (unsigned char *)_l;
  
  _l = new_l;
  _cap = want;
  _head = 0;
  _tail = _n;
  return true;
}

template <class T> void
DEQueue<T>::shrink(int nn)
{
  // delete els from back of queue
  if (nn < _n) {
    int num_to_del = _n - nn;
    for ( ; num_to_del > 0; _tail = prev_i(_tail), num_to_del--)
      _l[prev_i(_tail)].~T();
    _n = nn;
  }
}

template <class T> void
DEQueue<T>::resize(int nn, const T &e)
{
  // extra/excess els are added/removed to/from back of queue
  if (nn <= _cap || reserve(nn)) {
    // construct new els
    for ( ; _n < nn; _tail = next_i(_tail), _n++)
      new(velt(_tail)) T(e);

    // delete excess els
    for ( ; nn < _n; _tail = prev_i(_tail), _n--)
      _l[prev_i(_tail)].~T();
  }
}

template <class T> void
DEQueue<T>::swap(DEQueue<T> &o)
{
  T *l = _l;
  int n = _n;
  int cap = _cap;
  int head = _head;
  int tail = _tail;
  _l = o._l;
  _n = o._n;
  _cap = o._cap;
  _head = o._head;
  _tail = o._tail;
  o._l = l;
  o._n = n;
  o._cap = cap;
  o._head = head;
  o._tail = tail;
}

template <class T> inline void
DEQueue<T>::check_rep()
{
  // pushes go on to back (tail), pops come from front (head).
  // elements closer to the tail have larger indices in the array
  // (modulo total size).

  // _head is index of front
  // _tail is 1 + index of back, i.e. where next push goes

  if (!_l) {
    assert(_n == 0);
    assert(_cap == 0);
    assert(_head == 0);
    assert(_tail == 0);
  }
  else {
    assert(_head >= 0);
    assert(_head < _cap);
    assert(_tail >= 0);
    assert(_tail < _cap);
    if (_n == _cap)
      assert(_head == _tail);
    else
      assert(_n == (_tail >= _head ? _tail - _head : _cap - _head + _tail));
  }      
}



#endif

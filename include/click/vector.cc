/*
 * vector.{cc,hh} -- simple array template class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
#include <click/glue.hh>
#include <click/vector.hh>
CLICK_DECLS

template <class T>
Vector<T>::Vector(const Vector<T> &x)
    : _l(0), _n(0), _capacity(0)
{
    *this = x;
}

template <class T>
Vector<T>::~Vector()
{
    for (size_type i = 0; i < _n; i++)
	_l[i].~T();
    CLICK_LFREE(_l, sizeof(T) * _capacity);
}

template <class T> Vector<T> &
Vector<T>::operator=(const Vector<T> &o)
{
    if (&o != this) {
	for (size_type i = 0; i < _n; i++)
	    _l[i].~T();
#ifdef VALGRIND_MAKE_MEM_NOACCESS
	if (_l && _n)
	    VALGRIND_MAKE_MEM_NOACCESS(_l, _n * sizeof(T));
#endif
	_n = 0;
	if (reserve(o._n)) {
	    _n = o._n;
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
	    if (_l && _n)
		VALGRIND_MAKE_MEM_UNDEFINED(_l, _n * sizeof(T));
#endif
	    for (size_type i = 0; i < _n; i++)
		new(velt(i)) T(o._l[i]);
	}
    }
    return *this;
}

template <class T> Vector<T> &
Vector<T>::assign(size_type n, const T &e)
{
  resize(0, e);
  resize(n, e);
  return *this;
}

template <class T> typename Vector<T>::iterator
Vector<T>::insert(iterator i, const T& e)
{
    assert(i >= begin() && i <= end());
    if (_n == _capacity) {
	size_type pos = i - begin();
	if (!reserve(RESERVE_GROW))
	    return end();
	i = begin() + pos;
    }
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
    VALGRIND_MAKE_MEM_UNDEFINED(velt(_n), sizeof(T));
#endif
    for (iterator j = end(); j > i; ) {
	--j;
	new((void*) (j + 1)) T(*j);
	j->~T();
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
	VALGRIND_MAKE_MEM_UNDEFINED(j, sizeof(T));
#endif
    }
    new((void*) i) T(e);
    _n++;
    return i;
}

template <class T> typename Vector<T>::iterator
Vector<T>::erase(iterator a, iterator b)
{
  if (b > a) {
    assert(a >= begin() && b <= end());
    iterator i = a, j = b;
    for (; j < end(); i++, j++) {
      i->~T();
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
      VALGRIND_MAKE_MEM_UNDEFINED(i, sizeof(T));
#endif
      new((void*) i) T(*j);
    }
    for (; i < end(); i++)
      i->~T();
    _n -= b - a;
#ifdef VALGRIND_MAKE_MEM_NOACCESS
    VALGRIND_MAKE_MEM_NOACCESS(_l + _n, (b - a) * sizeof(T));
#endif
    return a;
  } else
    return b;
}

template <class T> bool
Vector<T>::reserve(size_type want)
{
  if (want < 0)
    want = (_capacity > 0 ? _capacity * 2 : 4);
  if (want <= _capacity)
    return true;

  T *new_l = (T *) CLICK_LALLOC(sizeof(T) * want);
  if (!new_l)
    return false;
#ifdef VALGRIND_MAKE_MEM_NOACCESS
  VALGRIND_MAKE_MEM_NOACCESS(new_l + _n, (want - _n) * sizeof(T));
#endif

  for (size_type i = 0; i < _n; i++) {
    new(velt(new_l, i)) T(_l[i]);
    _l[i].~T();
  }
  CLICK_LFREE(_l, sizeof(T) * _capacity);

  _l = new_l;
  _capacity = want;
  return true;
}

template <class T> void
Vector<T>::resize(size_type nn, const T &e)
{
  if (nn <= _capacity || reserve(nn)) {
    for (size_type i = nn; i < _n; i++)
      _l[i].~T();
#ifdef VALGRIND_MAKE_MEM_NOACCESS
    if (nn < _n)
	VALGRIND_MAKE_MEM_NOACCESS(_l + nn, (_n - nn) * sizeof(T));
    if (_n < nn)
	VALGRIND_MAKE_MEM_UNDEFINED(_l + _n, (nn - _n) * sizeof(T));
#endif
    for (size_type i = _n; i < nn; i++)
      new(velt(i)) T(e);
    _n = nn;
  }
}

template <class T> void
Vector<T>::swap(Vector<T> &x)
{
    T *l = _l;
    _l = x._l;
    x._l = l;

    size_type n = _n;
    _n = x._n;
    x._n = n;

    size_type cap = _capacity;
    _capacity = x._capacity;
    x._capacity = cap;
}

CLICK_ENDDECLS
#endif

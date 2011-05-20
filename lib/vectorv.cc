/*
 * vectorv.cc -- template specialization for Vector<void*>
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include <click/glue.hh>
#include <click/vector.hh>
CLICK_DECLS

Vector<void*>::Vector(const Vector<void*> &x)
    : _l(0), _n(0), _capacity(0)
{
    *this = x;
}

Vector<void*>::~Vector()
{
    CLICK_LFREE(_l, sizeof(void *) * _capacity);
}

Vector<void*> &
Vector<void*>::operator=(const Vector<void*> &o)
{
    if (&o != this) {
#ifdef VALGRIND_MAKE_MEM_NOACCESS
	if (_l && _n)
	    VALGRIND_MAKE_MEM_NOACCESS(_l, _n * sizeof(void *));
#endif
	_n = 0;
	if (reserve(o._n)) {
	    _n = o._n;
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
	    if (_l && _n)
		VALGRIND_MAKE_MEM_UNDEFINED(_l, _n * sizeof(void *));
#endif
	    memcpy(_l, o._l, sizeof(void *) * _n);
	}
    }
    return *this;
}

Vector<void*> &
Vector<void*>::assign(size_type n, void* e)
{
#ifdef VALGRIND_MAKE_MEM_NOACCESS
  if (_l && _n)
    VALGRIND_MAKE_MEM_NOACCESS(_l, _n * sizeof(void *));
#endif
  _n = 0;
  resize(n, e);
  return *this;
}

Vector<void*>::iterator
Vector<void*>::insert(iterator i, void* e)
{
  assert(i >= begin() && i <= end());
  size_type pos = i - begin();
  if (_n < _capacity || reserve(RESERVE_GROW)) {
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
    VALGRIND_MAKE_MEM_UNDEFINED(_l + _n, sizeof(void*));
#endif
    i = begin() + pos;
    memmove(i + 1, i, (end() - i) * sizeof(void*));
    *i = e;
    _n++;
  }
  return i;
}

Vector<void*>::iterator
Vector<void*>::erase(iterator a, iterator b)
{
  if (b > a) {
    assert(a >= begin() && b <= end());
    memmove(a, b, (end() - b) * sizeof(void*));
    _n -= b - a;
#ifdef VALGRIND_MAKE_MEM_NOACCESS
    VALGRIND_MAKE_MEM_NOACCESS(_l + _n, (b - a) * sizeof(void *));
#endif
    return a;
  } else
    return b;
}

bool
Vector<void*>::reserve(size_type want)
{
  if (want < 0)
    want = (_capacity > 0 ? _capacity * 2 : 4);
  if (want <= _capacity)
    return true;

  void** new_l = (void **) CLICK_LALLOC(sizeof(void *) * want);
  if (!new_l)
    return false;
#ifdef VALGRIND_MAKE_MEM_NOACCESS
  VALGRIND_MAKE_MEM_NOACCESS(new_l + _n, (want - _n) * sizeof(void *));
#endif

  memcpy(new_l, _l, sizeof(void*) * _n);
  CLICK_LFREE(_l, sizeof(void *) * _capacity);

  _l = new_l;
  _capacity = want;
  return true;
}

void
Vector<void*>::resize(size_type nn, void* e)
{
  if (nn <= _capacity || reserve(nn)) {
    assert(nn >= 0);
#ifdef VALGRIND_MAKE_MEM_NOACCESS
    if (nn < _n)
	VALGRIND_MAKE_MEM_NOACCESS(_l + nn, (_n - nn) * sizeof(void *));
    if (_n < nn)
	VALGRIND_MAKE_MEM_UNDEFINED(_l + _n, (nn - _n) * sizeof(void *));
#endif
    for (size_type i = _n; i < nn; i++)
      _l[i] = e;
    _n = nn;
  }
}

void
Vector<void*>::swap(Vector<void*> &x)
{
    void **l = _l;
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

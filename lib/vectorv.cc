/*
 * vectorv.cc -- template specialization for Vector<void *>
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/vector.hh>
#include <click/subvector.hh>

Vector<void *>::Vector(const Vector<void *> &o)
  : _l(0), _n(0), _cap(0)
{
  *this = o;
}

Vector<void *>::Vector(const Subvector<void *> &o)
  : _l(0), _n(0), _cap(0)
{
  if (reserve(o._n)) {
    _n = o._n;
    memcpy(_l, o._l, sizeof(void *) * _n);
  }
}

Vector<void *>::~Vector()
{
  delete[] _l;
}

Vector<void *> &
Vector<void *>::operator=(const Vector<void *> &o)
{
  if (&o != this) {
    _n = 0;
    if (reserve(o._n)) {
      _n = o._n;
      memcpy(_l, o._l, sizeof(void *) * _n);
    }
  }
  return *this;
}

Vector<void *> &
Vector<void *>::assign(int n, void *e)
{
  _n = 0;
  if (reserve(n)) {
    _n = n;
    for (int i = 0; i < _n; i++)
      _l[i] = e;
  }
  return *this;
}

bool
Vector<void *>::reserve(int want)
{
  if (want < 0)
    want = _cap > 0 ? _cap * 2 : 4;
  if (want <= _cap)
    return true;
  
  void **new_l = new void *[want];
  if (!new_l)
    return false;
  
  memcpy(new_l, _l, sizeof(void *) * _n);
  delete[] _l;
  
  _l = new_l;
  _cap = want;
  return true;
}

void
Vector<void *>::resize(int nn, void *e)
{
  if (nn <= _cap || reserve(nn)) {
    for (int i = _n; i < nn; i++)
      _l[i] = e;
    _n = nn;
  }
}

void
Vector<void *>::swap(Vector<void *> &o)
{
  void **l = _l;
  int n = _n;
  int cap = _cap;
  _l = o._l;
  _n = o._n;
  _cap = o._cap;
  o._l = l;
  o._n = n;
  o._cap = cap;
}

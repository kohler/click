#include "vector.hh"
#include "glue.hh"

template <class T>
Vector<T>::Vector(const Vector<T> &o)
  : _l(0), _n(0), _cap(0)
{
  *this = o;
}

template <class T>
Vector<T>::~Vector()
{
  for (int i = 0; i < _n; i++)
    _l[i].~T();
  delete[] (unsigned char *)_l;
}

template <class T> Vector<T> &
Vector<T>::operator=(const Vector<T> &o)
{
  if (&o != this) {
    for (int i = 0; i < _n; i++)
      _l[i].~T();
    _n = 0;
    if (reserve(o._n)) {
      _n = o._n;
      for (int i = 0; i < _n; i++)
        new(velt(i)) T(o._l[i]);
    }
  }
  return *this;
}

template <class T> Vector<T> &
Vector<T>::assign(int n, const T &e)
{
  resize(0, e);
  resize(n, e);
  return *this;
}

template <class T> bool
Vector<T>::reserve(int want)
{
  if (want < 0)
    want = _cap > 0 ? _cap * 2 : 4;
  if (want <= _cap)
    return true;
  _cap = want;
  
  T *new_l = (T *)new unsigned char[sizeof(T) * _cap];
  if (!new_l) return false;
  
  for (int i = 0; i < _n; i++) {
    new(velt(new_l, i)) T(_l[i]);
    _l[i].~T();
  }
  delete[] (unsigned char *)_l;
  _l = new_l;
  return true;
}

template <class T> void
Vector<T>::resize(int nn, const T &e)
{
  if (nn <= _cap || reserve(nn)) {
    for (int i = nn; i < _n; i++)
      _l[i].~T();
    for (int i = _n; i < nn; i++)
      new(velt(i)) T(e);
    _n = nn;
  }
}

template <class T> void
Vector<T>::swap(Vector<T> &o)
{
  T *l = _l;
  int n = _n;
  int cap = _cap;
  _l = o._l;
  _n = o._n;
  _cap = o._cap;
  o._l = l;
  o._n = n;
  o._cap = cap;
}

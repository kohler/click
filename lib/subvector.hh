#ifndef SUBVECTOR_HH
#define SUBVECTOR_HH
#include "vector.hh"

template <class T>
class Subvector {
  
  T *_l;
  int _n;

  Subvector<T> &operator=(const Subvector<T> &);

  friend class Vector<T>;
  
 public:
  
  Subvector(const Vector<T> &, int, int);
  Subvector(const Subvector<T> &o)	: _l(o._l), _n(o._n) { }
  ~Subvector()				{ }
  
  int size() const			{ return _n; }
  
  const T &at(int i) const		{ assert(i>=0 && i<_n); return _l[i]; }
  const T &operator[](int i) const	{ return at(i); }
  const T &back() const			{ return at(_n - 1); }
  const T &at_u(int i) const		{ return _l[i]; }
  
  T &at(int i)				{ assert(i>=0 && i<_n); return _l[i]; }
  T &operator[](int i)			{ return at(i); }
  T &back()				{ return at(_n - 1); }
  T &at_u(int i)			{ return _l[i]; }
  
};

template <class T> inline
Subvector<T>::Subvector(const Vector<T> &o, int i, int j)
{
  int s = o.size();
  if (i < 0 || j <= 0 || i >= s) {
    _l = 0;
    _n = 0;
  } else {
    _l = o._l + i;
    _n = (i + j > s ? s - i : j);
  }
}


template <>
class Subvector<void *> {
  
  void **_l;
  int _n;

  Subvector<void *> &operator=(const Subvector<void *> &);
  
  friend class Vector<void *>;
  
 public:
  
  Subvector(const Vector<void *> &, int, int);
  Subvector(const Subvector<void *> &o)	: _l(o._l), _n(o._n) { }
  ~Subvector()				{ }
  
  int size() const			{ return _n; }
  
  void *at(int i) const			{ assert(i>=0 && i<_n); return _l[i]; }
  void *operator[](int i) const		{ return at(i); }
  void *back() const			{ return at(_n - 1); }
  void *at_u(int i) const		{ return _l[i]; }
  
  void *&at(int i)			{ assert(i>=0 && i<_n); return _l[i]; }
  void *&operator[](int i)		{ return at(i); }
  void *&back()				{ return at(_n - 1); }
  void *&at_u(int i)			{ return _l[i]; }
  
};

inline
Subvector<void *>::Subvector(const Vector<void *> &o, int i, int j)
{
  int s = o.size();
  if (i < 0 || j <= 0 || i >= s) {
    _l = 0;
    _n = 0;
  } else {
    _l = o._l + i;
    _n = (i + j > s ? s - i : j);
  }
}


template <class T>
class Subvector<T *>: private Subvector<void *> {
  
  typedef Vector<void *> Base;

  Subvector<T *> &operator=(const Subvector<T *> &);
  
 public:
  
  Subvector(const Vector<T *> &o, int i, int j) : Base(o, i, j) { }
  Subvector(const Subvector<T *> &o)		: Base(o) { }
  ~Subvector()			{ }
  
  int size() const		{ return Base::size(); }
  
  T *operator[](int i) const	{ return (T *)(Base::at(i)); }
  T *at(int i) const		{ return (T *)(Base::at(i)); }
  T *back() const		{ return (T *)(Base::back()); }
  T *at_u(int i) const		{ return (T *)(Base::at_u(i)); }
  
  T *&operator[](int i)		{ return (T *&)(Base::at(i)); }
  T *&at(int i)			{ return (T *&)(Base::at(i)); }
  T *&back()			{ return (T *&)(Base::back()); }
  T *&at_u(int i)		{ return (T *&)(Base::at_u(i)); }
  
};

#endif

#ifndef CLICK_VECTOR_HH
#define CLICK_VECTOR_HH
template <class T> class Subvector;

template <class T>
class Vector { public:

  Vector()			: _l(0), _n(0), _cap(0) { }
  explicit Vector(int capacity)	: _l(0), _n(0), _cap(0) { reserve(capacity); }
  Vector(int n, const T &e)	: _l(0), _n(0), _cap(0) { resize(n, e); }
  Vector(const Vector<T> &);
  Vector(const Subvector<T> &);
  ~Vector();
  
  int size() const			{ return _n; }
  
  const T &at(int i) const		{ assert(i>=0 && i<_n); return _l[i]; }
  const T &operator[](int i) const	{ return at(i); }
  const T &back() const			{ return at(_n - 1); }
  const T &at_u(int i) const		{ return _l[i]; }
  
  T &at(int i)				{ assert(i>=0 && i<_n); return _l[i]; }
  T &operator[](int i)			{ return at(i); }
  T &back()				{ return at(_n - 1); }
  T &at_u(int i)			{ return _l[i]; }
  
  void push_back(const T &);
  void pop_back();
  
  void clear()				{ resize(0); }
  bool reserve(int);
  void resize(int nn, const T &e = T());
  
  Vector<T> &operator=(const Vector<T> &);
  Vector<T> &assign(int n, const T &e = T());
  void swap(Vector<T> &);

 private:
  
  T *_l;
  int _n;
  int _cap;

  void *velt(int i) const		{ return (void *)&_l[i]; }
  static void *velt(T *l, int i)	{ return (void *)&l[i]; }

  friend class Subvector<T>;
  
};

template <class T> inline void
Vector<T>::push_back(const T &e)
{
  if (_n < _cap || reserve(-1)) {
    new(velt(_n)) T(e);
    _n++;
  }
}

template <class T> inline void
Vector<T>::pop_back()
{
  assert(_n > 0);
  --_n;
  _l[_n].~T();
}


template <>
class Vector<void *> { public:
  
  Vector()			: _l(0), _n(0), _cap(0) { }
  explicit Vector(int capacity)	: _l(0), _n(0), _cap(0) { reserve(capacity); }
  Vector(int n, void *e)	: _l(0), _n(0), _cap(0) { resize(n, e); }
  Vector(const Vector<void *> &);
  Vector(const Subvector<void *> &);
  ~Vector();
  
  int size() const			{ return _n; }
  
  void *at(int i) const			{ assert(i>=0 && i<_n); return _l[i]; }
  void *operator[](int i) const		{ return at(i); }
  void *back() const			{ return at(_n - 1); }
  void *at_u(int i) const		{ return _l[i]; }
  
  void *&at(int i)			{ assert(i>=0 && i<_n); return _l[i]; }
  void *&operator[](int i)		{ return at(i); }
  void *&back()				{ return at(_n - 1); }
  void *&at_u(int i)			{ return _l[i]; }
  
  void push_back(void *);
  void pop_back();
  
  void clear()				{ _n = 0; }
  bool reserve(int);
  void resize(int nn, void *e = 0);
  
  Vector<void *> &operator=(const Vector<void *> &);
  Vector<void *> &assign(int n, void *e = 0);
  void swap(Vector<void *> &);

 private:
  
  void **_l;
  int _n;
  int _cap;

  friend class Subvector<void *>;
  
};

inline void
Vector<void *>::push_back(void *e)
{
  if (_n < _cap || reserve(-1)) {
    _l[_n] = e;
    _n++;
  }
}

inline void
Vector<void *>::pop_back()
{
  assert(_n > 0);
  --_n;
}


template <class T>
class Vector<T *>: private Vector<void *> {
  
  typedef Vector<void *> Base;
  
 public:
  
  Vector()			: Base() { }
  explicit Vector(int capacity)	: Base(capacity) { }
  Vector(int n, T *e)		: Base(n, (void *)e) { }
  Vector(const Vector<T *> &o)	: Base(o) { }
  Vector(const Subvector<T *> &o) : Base(o) { }
  ~Vector()			{ }
  
  int size() const		{ return Base::size(); }
  
  T *operator[](int i) const	{ return (T *)(Base::at(i)); }
  T *at(int i) const		{ return (T *)(Base::at(i)); }
  T *back() const		{ return (T *)(Base::back()); }
  T *at_u(int i) const		{ return (T *)(Base::at_u(i)); }
  
  T *&operator[](int i)		{ return (T *&)(Base::at(i)); }
  T *&at(int i)			{ return (T *&)(Base::at(i)); }
  T *&back()			{ return (T *&)(Base::back()); }
  T *&at_u(int i)		{ return (T *&)(Base::at_u(i)); }
  
  void push_back(T *e)		{ Base::push_back((void *)e); }
  void pop_back()		{ Base::pop_back(); }
  
  void clear()			{ Base::clear(); }
  bool reserve(int n)		{ return Base::reserve(n); }
  void resize(int n, T *e = 0)	{ Base::resize(n, (void *)e); }
  
  Vector<T *> &operator=(const Vector<T *> &o)
  		{ Base::operator=(o); return *this; }
  Vector<T *> &assign(int n, T *e = 0)
  		{ Base::assign(n, (void *)e); return *this; }
  void swap(Vector<T *> &o)	{ Base::swap(o); }
  
};

#endif

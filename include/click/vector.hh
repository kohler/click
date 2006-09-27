#ifndef CLICK_VECTOR_HH
#define CLICK_VECTOR_HH
CLICK_DECLS

template <class T>
class Vector { public:

  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;
  
  typedef int size_type;
  enum { RESERVE_GROW = (size_type) -1 };
  
  typedef T* iterator;
  typedef const T* const_iterator;
  
  explicit Vector()		: _l(0), _n(0), _capacity(0) { }
  explicit Vector(size_type n, const T &e) : _l(0), _n(0), _capacity(0) { resize(n, e); }
  // template <class In> ...
  Vector(const Vector<T> &);
  ~Vector();

  Vector<T>& operator=(const Vector<T>&);
  Vector<T>& assign(size_type n, const T& e = T());
  // template <class In> ...
  
  // iterators
  iterator begin()			{ return _l; }
  const_iterator begin() const		{ return _l; }
  iterator end()			{ return _l + _n; }
  const_iterator end() const		{ return _l + _n; }

  // capacity
  size_type size() const		{ return _n; }
  void resize(size_type nn, const T& e = T());
  size_type capacity() const		{ return _capacity; }
  bool empty() const			{ return _n == 0; }
  bool reserve(size_type);

  // element access
  T& operator[](size_type i)		{ assert(i>=0 && i<_n); return _l[i]; }
  const T& operator[](size_type i) const{ assert(i>=0 && i<_n); return _l[i]; }
  T& at(size_type i)			{ return operator[](i); }
  const T& at(size_type i) const	{ return operator[](i); }
  T& front()				{ return operator[](0); }
  const T& front() const		{ return operator[](0); }
  T& back()				{ return operator[](_n - 1); }
  const T& back() const			{ return operator[](_n - 1); }
  T& at_u(size_type i)			{ return _l[i]; }
  const T& at_u(size_type i) const	{ return _l[i]; }

  // modifiers
  inline void push_back(const T&);
  inline void pop_back();
  iterator insert(iterator, const T&);
  inline iterator erase(iterator);
  iterator erase(iterator, iterator);
  void swap(Vector<T> &);
  void clear()				{ erase(begin(), end()); }
  
 private:
  
  T *_l;
  size_type _n;
  size_type _capacity;

  void *velt(size_type i) const		{ return (void*)&_l[i]; }
  static void *velt(T* l, size_type i)	{ return (void*)&l[i]; }

};

template <class T> inline void
Vector<T>::push_back(const T& e)
{
  if (_n < _capacity || reserve(RESERVE_GROW)) {
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

template <class T> inline typename Vector<T>::iterator
Vector<T>::erase(iterator i)
{
  return (i < end() ? erase(i, i + 1) : i);
}


template <>
class Vector<void*> { public:

  typedef void* value_type;
  typedef void*& reference;
  typedef void* const& const_reference;
  typedef void** pointer;
  typedef void* const* const_pointer;

  typedef int size_type;
  enum { RESERVE_GROW = (size_type) -1 };
  
  typedef void** iterator;
  typedef void* const* const_iterator;

  explicit Vector()			: _l(0), _n(0), _capacity(0) { }
  explicit Vector(size_type n, void* e)	: _l(0), _n(0), _capacity(0) { resize(n, e); }
  Vector(const Vector<void*> &);
  ~Vector();
  
  Vector<void*> &operator=(const Vector<void*> &);
  Vector<void*> &assign(size_type n, void* e = 0);
  
  // iterators
  iterator begin()			{ return _l; }
  const_iterator begin() const		{ return _l; }
  iterator end()			{ return _l + _n; }
  const_iterator end() const		{ return _l + _n; }

  // capacity
  size_type size() const		{ return _n; }
  void resize(size_type nn, void* e = 0);
  size_type capacity() const		{ return _capacity; }
  bool empty() const			{ return _n == 0; }
  bool reserve(size_type);

  // element access
  void*& operator[](size_type i)	{ assert(i>=0 && i<_n); return _l[i]; }
  void* operator[](size_type i) const	{ assert(i>=0 && i<_n); return _l[i]; }
  void*& at(size_type i)		{ return operator[](i); }
  void* at(size_type i) const		{ return operator[](i); }
  void*& front()			{ return operator[](0); }
  void* front() const			{ return operator[](0); }
  void*& back()				{ return operator[](_n - 1); }
  void* back() const			{ return operator[](_n - 1); }
  void*& at_u(size_type i)		{ return _l[i]; }
  void* at_u(size_type i) const		{ return _l[i]; }

  // modifiers
  inline void push_back(void*);
  inline void pop_back();
  iterator insert(iterator, void*);
  inline iterator erase(iterator);
  iterator erase(iterator, iterator);
  void swap(Vector<void*> &);
  void clear()				{ _n = 0; }

 private:
  
  void **_l;
  size_type _n;
  size_type _capacity;

};

inline void
Vector<void*>::push_back(void *e)
{
  if (_n < _capacity || reserve(RESERVE_GROW)) {
    _l[_n] = e;
    _n++;
  }
}

inline void
Vector<void*>::pop_back()
{
  assert(_n > 0);
  --_n;
}

inline Vector<void*>::iterator 
Vector<void*>::erase(Vector<void*>::iterator e)
{
  return (e < end() ? erase(e, e + 1) : e);
}


template <class T>
class Vector<T*>: private Vector<void*> {
  
  typedef Vector<void*> Base;
  
 public:

  typedef T* value_type;
  typedef T*& reference;
  typedef T* const& const_reference;
  typedef T** pointer;
  typedef T* const* const_pointer;

  typedef int size_type;
  enum { RESERVE_GROW = Base::RESERVE_GROW };
  
  typedef T** iterator;
  typedef T* const* const_iterator;
  
  explicit Vector()			: Base() { }
  explicit Vector(size_type n, T* e)	: Base(n, (void *)e) { }
  Vector(const Vector<T *> &o)		: Base(o) { }
  ~Vector()				{ }

  Vector<T *> &operator=(const Vector<T *> &o)
  		{ Base::operator=(o); return *this; }
  Vector<T *> &assign(size_type n, T *e = 0)
  		{ Base::assign(n, (void *)e); return *this; }
  
  // iterators
  const_iterator begin() const	{ return (const_iterator)(Base::begin()); }
  iterator begin()		{ return (iterator)(Base::begin()); }
  const_iterator end() const	{ return (const_iterator)(Base::end()); }
  iterator end()		{ return (iterator)(Base::end()); }

  // capacity
  size_type size() const	{ return Base::size(); }
  void resize(size_type n, T *e = 0) { Base::resize(n, (void *)e); }
  size_type capacity() const	{ return Base::capacity(); }
  bool empty() const		{ return Base::empty(); }
  bool reserve(size_type n)	{ return Base::reserve(n); }

  // element access
  T*& operator[](size_type i)	{ return (T*&)(Base::at(i)); }
  T* operator[](size_type i) const { return (T*)(Base::operator[](i)); }
  T*& at(size_type i)		{ return (T*&)(Base::operator[](i)); }
  T* at(size_type i) const	{ return (T*)(Base::at(i)); }
  T*& front()			{ return (T*&)(Base::front()); }
  T* front() const		{ return (T*)(Base::front()); }
  T*& back()			{ return (T*&)(Base::back()); }
  T* back() const		{ return (T*)(Base::back()); }
  T*& at_u(size_type i)		{ return (T*&)(Base::at_u(i)); }
  T* at_u(size_type i) const	{ return (T*)(Base::at_u(i)); }
  
  // modifiers
  void push_back(T* e)		{ Base::push_back((void*)e); }
  void pop_back()		{ Base::pop_back(); }
  iterator insert(iterator i, T* e) { return (iterator)Base::insert((void**)i, (void*)e); }
  iterator erase(iterator i)	{ return (iterator)Base::erase((void**)i); }
  iterator erase(iterator i, iterator j) { return (iterator)Base::erase((void**)i, (void**)j); }
  void swap(Vector<T *> &o)	{ Base::swap(o); }
  void clear()			{ Base::clear(); }
    
};

CLICK_ENDDECLS
#endif

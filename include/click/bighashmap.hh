#ifndef CLICK_BIGHASHMAP_HH
#define CLICK_BIGHASHMAP_HH
#include <click/bighashmap_arena.hh>
CLICK_DECLS

// K AND V REQUIREMENTS:
//
//		K::K(const K &)
//		k1 == k2
// int		hashcode(const K &)
//			If hashcode(k1) != hashcode(k2), then k1 != k2.
//
//		V::V() -- only used for default value
// 		V::V(const V &)
// V &		V::operator=(const V &)

template <class K, class V> class _BigHashMap_const_iterator;
template <class K, class V> class _BigHashMap_iterator;

template <class K, class V>
class BigHashMap { public:
  
  BigHashMap();
  explicit BigHashMap(const V &);
  ~BigHashMap();
  
  int size() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  int nbuckets() const			{ return _nbuckets; }
  
  const V &find(const K &) const;
  V *findp(const K &) const;
  const V &operator[](const K &k) const;
  V &find_force(const K &);
  V *findp_force(const K &);
  
  bool insert(const K &, const V &);
  bool remove(const K &);
  void clear();

  void swap(BigHashMap<K, V> &);

  // iteration
  typedef _BigHashMap_const_iterator<K, V> const_iterator;
  typedef _BigHashMap_iterator<K, V> iterator;
  const_iterator first() const;
  iterator first();
  
  typedef iterator Iterator;

  // dynamic resizing
  void resize(int);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);

  // dangerous operations
  static const K &key_of_value(V *v)	{ return *(const K *)((char *)v - offsetof(Elt, v)); }
  
 private:
  
  struct Elt {
    K key;
    V v;
    Elt *next;
  };

  Elt **_buckets;
  int _nbuckets;
  V _default_v;

  int _n;
  int _capacity;

  Elt *_free;
  int _free_arena;
  BigHashMap_Arena **_arenas;
  int _narenas;
  int _arenas_cap;

  void initialize();
  void resize0(int = -1);
  int bucket(const K &) const;
  Elt *find_elt(const K &) const;

  Elt *alloc();
  Elt *slow_alloc();
  void free(Elt *);

  enum { MAX_NBUCKETS = 32767 };
  
  friend class _BigHashMap_iterator<K, V>;
  friend class _BigHashMap_const_iterator<K, V>;
  
};

template <class K, class V>
class _BigHashMap_const_iterator { public:

  _BigHashMap_const_iterator(const BigHashMap<K, V> *m);

  operator bool() const			{ return _elt; }
  void operator++(int);
  void operator++()			{ (*this)++; }
  
  const K &key() const			{ return _elt->key; }
  const V &value() const		{ return _elt->v; }

 private:

  const BigHashMap<K, V> *_hm;
  typename BigHashMap<K, V>::Elt *_elt;
  int _bucket;

};

template <class K, class V>
class _BigHashMap_iterator : public _BigHashMap_const_iterator<K, V> { public:

  typedef _BigHashMap_const_iterator<K, V> inherited;
  
  _BigHashMap_iterator(BigHashMap<K, V> *m) : inherited(m) { }

  V &value() const		{ return const_cast<V &>(inherited::value()); }
  
};

template <class K, class V>
inline typename BigHashMap<K, V>::const_iterator
BigHashMap<K, V>::first() const
{
  return const_iterator(this);
}

template <class K, class V>
inline typename BigHashMap<K, V>::iterator
BigHashMap<K, V>::first()
{
  return iterator(this);
}

template <class K, class V>
inline const V &
BigHashMap<K, V>::find(const K &key) const
{
  Elt *e = find_elt(key);
  const V *v = (e ? &e->v : &_default_v);
  return *v;
}

template <class K, class V>
inline const V &
BigHashMap<K, V>::operator[](const K &key) const
{
  return find(key);
}

template <class K, class V>
inline V *
BigHashMap<K, V>::findp(const K &key) const
{
  Elt *e = find_elt(key);
  return e ? &e->v : 0;
}

template <class K, class V>
inline V &
BigHashMap<K, V>::find_force(const K &key)
{
  return *findp_force(key);
}


template <class K>
class BigHashMap<K, void *> { public:

  BigHashMap();
  explicit BigHashMap(void *);
  ~BigHashMap();
  
  int nbuckets() const			{ return _nbuckets; }
  int size() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  
  void *find(const K &) const;
  void **findp(const K &) const;
  void *operator[](const K &k) const;
  void *&find_force(const K &);
  void **findp_force(const K &);
  
  bool insert(const K &, void *);
  bool remove(const K &);
  void clear();

  void swap(BigHashMap<K, void *> &);

  // iterators
  typedef _BigHashMap_const_iterator<K, void *> const_iterator;
  typedef _BigHashMap_iterator<K, void *> iterator;

  const_iterator first() const;
  iterator first();

  typedef iterator Iterator;
  
  // dynamic resizing
  void resize(int);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);

  // dangerous operations
  static const K &key_of_value(void **v) { return *(const K *)((char *)v - offsetof(Elt, v)); }
  
 private:
  
  struct Elt {
    K key;
    void *v;
    Elt *next;
  };

  Elt **_buckets;
  int _nbuckets;
  void *_default_v;

  int _n;
  int _capacity;

  Elt *_free;
  int _free_arena;
  BigHashMap_Arena **_arenas;
  int _narenas;
  int _arenas_cap;

  void initialize();
  void resize0(int = -1);
  int bucket(const K &) const;
  Elt *find_elt(const K &) const;

  Elt *alloc();
  Elt *slow_alloc();
  void free(Elt *);

  enum { MAX_NBUCKETS = 32767 };
  
  friend class iterator;
  friend class const_iterator;
  
};

template <class K>
class _BigHashMap_const_iterator<K, void *> { public:

  _BigHashMap_const_iterator(const BigHashMap<K, void *> *);

  operator bool() const			{ return _elt; }
  void operator++(int);
  void operator++()			{ (*this)++; }
  
  const K &key() const			{ return _elt->key; }
  void *value() const			{ return _elt->v; }
  
 private:

  const BigHashMap<K, void *> *_hm;
  typename BigHashMap<K, void *>::Elt *_elt;
  int _bucket;

  template <class, class> friend class _BigHashMap_iterator;

};

template <class K>
class _BigHashMap_iterator<K, void *> : public _BigHashMap_const_iterator<K, void *> { public:

  typedef _BigHashMap_const_iterator<K, void *> inherited;

  _BigHashMap_iterator(BigHashMap<K, void *> *m) : inherited(m) { }
  
  void *&value() const			{ return _elt->v; }

};

template <class K>
inline typename BigHashMap<K, void *>::const_iterator
BigHashMap<K, void *>::first() const
{
  return const_iterator(this);
}

template <class K>
inline typename BigHashMap<K, void *>::iterator
BigHashMap<K, void *>::first()
{
  return iterator(this);
}

template <class K>
inline void *
BigHashMap<K, void *>::find(const K &key) const
{
  Elt *e = find_elt(key);
  return (e ? e->v : _default_v);
}

template <class K>
inline void *
BigHashMap<K, void *>::operator[](const K &key) const
{
  return find(key);
}

template <class K>
inline void **
BigHashMap<K, void *>::findp(const K &key) const
{
  Elt *e = find_elt(key);
  return e ? &e->v : 0;
}

template <class K>
inline void *&
BigHashMap<K, void *>::find_force(const K &key)
{
  return *findp_force(key);
}


template <class K, class T>
class BigHashMap<K, T *> : public BigHashMap<K, void *> { public:

  typedef BigHashMap<K, void *> inherited;
  
  BigHashMap()				: inherited() { }
  explicit BigHashMap(T *def)		: inherited(def) { }
  ~BigHashMap()				{ }
  
  int nbuckets() const			{ return inherited::nbuckets(); }
  int size() const			{ return inherited::size(); }
  bool empty() const			{ return inherited::empty(); }
  
  T *find(const K &k) const { return reinterpret_cast<T *>(inherited::find(k)); }
  T **findp(const K &k) const { return reinterpret_cast<T **>(inherited::findp(k)); }
  T *operator[](const K &k) const { return reinterpret_cast<T *>(inherited::operator[](k)); }
  T *&find_force(const K &k) { return reinterpret_cast<T *>(inherited::find_force(k)); }
  
  bool insert(const K &k, T *v)		{ return inherited::insert(k, v); }
  bool remove(const K &k)		{ return inherited::remove(k); }
  void clear()				{ inherited::clear(); }

  void swap(BigHashMap<K, T *> &o)	{ inherited::swap(o); }

  // iteration
  typedef _BigHashMap_const_iterator<K, T *> const_iterator;
  typedef _BigHashMap_iterator<K, T *> iterator;
  
  const_iterator first() const;
  iterator first();

  typedef iterator Iterator;

  // dynamic resizing
  void resize(int s)			{ inherited::resize(s); }
  bool dynamic_resizing() const		{ return inherited::dynamic_resizing(); }
  void set_dynamic_resizing(bool dr)	{ inherited::set_dynamic_resizing(dr); }

};

template <class K, class T>
class _BigHashMap_const_iterator<K, T *> : private _BigHashMap_const_iterator<K, void *> { public:

  typedef _BigHashMap_const_iterator<K, void *> inherited;

  _BigHashMap_const_iterator(const BigHashMap<K, T *> *t) : inherited(t) { }

  operator bool() const	{ return inherited::operator bool(); }
  void operator++(int)	{ return inherited::operator++(0); }
  void operator++()	{ return inherited::operator++(); }
  
  const K &key() const	{ return inherited::key(); }
  T *value() const	{ return reinterpret_cast<T *>(inherited::value()); }

  friend class _BigHashMap_iterator<K, T *>;
  
};

template <class K, class T>
class _BigHashMap_iterator<K, T *> : public _BigHashMap_const_iterator<K, T *> { public:

  typedef _BigHashMap_const_iterator<K, T *> inherited;

  _BigHashMap_iterator(BigHashMap<K, T *> *t) : inherited(t) { }

  T *&value() const	{ return reinterpret_cast<T *&>(_elt->v); }
  
};

template <class K, class T>
inline typename BigHashMap<K, T *>::const_iterator
BigHashMap<K, T *>::first() const
{
  return const_iterator(this);
}

template <class K, class T>
inline typename BigHashMap<K, T *>::iterator
BigHashMap<K, T *>::first()
{
  return iterator(this);
}

CLICK_ENDDECLS
#endif

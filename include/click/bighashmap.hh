#ifndef CLICK_BIGHASHMAP_HH
#define CLICK_BIGHASHMAP_HH
#include <click/bighashmap_arena.hh>

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


template <class K, class V> class BigHashMapIterator;

template <class K, class V>
class BigHashMap { public:

  typedef BigHashMapIterator<K, V> Iterator;
  
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
  
  bool insert(const K &, const V &);
  bool remove(const K &);
  void clear();

  void swap(BigHashMap<K, V> &);
  
  Iterator first() const		{ return Iterator(this); }

  // dynamic resizing
  void resize(int);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);
  
 private:
  
  struct Elt {
    K k;
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

  static const int MAX_NBUCKETS = 32768;
  
  friend class BigHashMapIterator<K, V>;
  
};

template <class K, class V>
class BigHashMapIterator { public:

  BigHashMapIterator(const BigHashMap<K, V> *);

  operator bool() const			{ return _elt; }
  void operator++(int = 0);
  
  const K &key() const			{ return _elt->k; }
  const V &value() const		{ return _elt->v; }
  
 private:

  const BigHashMap<K, V> *_hm;
  BigHashMap<K, V>::Elt *_elt;
  int _bucket;

};

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


template <class K>
class BigHashMap<K, void *> { public:

  typedef BigHashMapIterator<K, void *> Iterator;
  
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
  
  bool insert(const K &, void *);
  bool remove(const K &);
  void clear();

  void swap(BigHashMap<K, void *> &);
  
  Iterator first() const;

  // dynamic resizing
  void resize(int);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);
  
 private:
  
  struct Elt {
    K k;
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

  static const int MAX_NBUCKETS = 32768;
  
  friend class BigHashMapIterator<K, void *>;
  
};

template <class K>
class BigHashMapIterator<K, void *> { public:

  BigHashMapIterator(const BigHashMap<K, void *> *);

  operator bool() const			{ return _elt; }
  void operator++(int = 0);
  
  const K &key() const			{ return _elt->k; }
  void *value() const			{ return _elt->v; }
  
 private:

  const BigHashMap<K, void *> *_hm;
  BigHashMap<K, void *>::Elt *_elt;
  int _bucket;

};

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
inline BigHashMapIterator<K, void *>
BigHashMap<K, void *>::first() const
{
  return Iterator(this);
}


template <class K, class T>
class BigHashMap<K, T *> : public BigHashMap<K, void *> { public:

  typedef BigHashMapIterator<K, T *> Iterator;
  typedef BigHashMap<K, void *> Base;
  
  BigHashMap()				: Base() { }
  explicit BigHashMap(T *def)		: Base(def) { }
  ~BigHashMap()				{ }
  
  int nbuckets() const			{ return Base::nbuckets(); }
  int size() const			{ return Base::size(); }
  bool empty() const			{ return Base::empty(); }
  
  T *find(const K &k) const { return reinterpret_cast<T *>(Base::find(k)); }
  T **findp(const K &k) const { return reinterpret_cast<T **>(Base::findp(k)); }
  T *operator[](const K &k) const { return reinterpret_cast<T *>(Base::operator[](k)); }
  T *&find_force(const K &k) { return reinterpret_cast<T *>(Base::find_force(k)); }
  
  bool insert(const K &k, T *v)		{ return Base::insert(k, v); }
  bool remove(const K &k)		{ return Base::remove(k); }
  void clear()				{ Base::clear(); }

  void swap(BigHashMap<K, T *> &o)	{ Base::swap(o); }
  
  Iterator first() const		{ return Iterator(this); }

  // dynamic resizing
  void resize(int s)			{ Base::resize(s); }
  bool dynamic_resizing() const		{ return Base::dynamic_resizing(); }
  void set_dynamic_resizing(bool dr)	{ Base::set_dynamic_resizing(dr); }

};

template <class K, class T>
class BigHashMapIterator<K, T *> : public BigHashMapIterator<K, void *> { public:

  typedef BigHashMapIterator<K, void *> Base;

  BigHashMapIterator(const BigHashMap<K, T *> *t) : Base(t) { }

  operator bool() const			{ return Base::operator bool(); }
  void operator++(int)			{ Base::operator++(0); }
  
  const K &key() const	{ return Base::key(); }
  T *value() const	{ return reinterpret_cast<T *>(Base::value()); }
  
};

#endif

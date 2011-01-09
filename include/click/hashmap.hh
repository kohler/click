#ifndef CLICK_HASHMAP_HH
#define CLICK_HASHMAP_HH
#include <click/hashcode.hh>
CLICK_DECLS
/** @cond never */
class HashMap_Arena;
class HashMap_ArenaFactory;

// K AND V REQUIREMENTS:
//
//		K::K(const K &)
//		k1 == k2
// hashcode_t	hashcode(const K &)
//			If hashcode(k1) != hashcode(k2), then k1 != k2.
//			hashcode() can return any unsigned value.
//
//		V::V() -- only used for default value
//		V::V(const V &)
// V &		V::operator=(const V &)

template <class K, class V = void> class _HashMap_const_iterator;
template <class K, class V = void> class _HashMap_iterator;
template <class K, class V = void> class HashMap;

template <class K, class V>
class HashMap { public:

    typedef K key_type;
    typedef V mapped_type;
    struct Pair;

    HashMap();
    explicit HashMap(const V &, HashMap_ArenaFactory * = 0);
    HashMap(const HashMap<K, V> &);
    ~HashMap();

  void set_arena(HashMap_ArenaFactory *);

  size_t size() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  size_t nbuckets() const		{ return _nbuckets; }

  Pair *find_pair(const K &) const;
  inline V *findp(const K &) const;
  inline const V &find(const K &, const V &) const;
  inline const V &find(const K &) const;
  inline const V &operator[](const K &) const;

  Pair *find_pair_force(const K &, const V &);
  Pair *find_pair_force(const K &k) { return find_pair_force(k, _default_value); }
  V *findp_force(const K &k, const V &v) { if (Pair *p = find_pair_force(k, v)) return &p->value; else return 0; }
  V &find_force(const K &k, const V &v) { return *findp_force(k, v); }
  V *findp_force(const K &k)	{ return findp_force(k, _default_value); }
  V &find_force(const K &k)	{ return *findp_force(k, _default_value); }

  bool insert(const K &, const V &);
  bool erase(const K &);
  bool remove(const K &key) {
    return erase(key);
  }
  void clear();

  void swap(HashMap<K, V> &);

  // iteration
  typedef _HashMap_const_iterator<K, V> const_iterator;
  typedef _HashMap_iterator<K, V> iterator;
  inline const_iterator begin() const;
  inline iterator begin();
  inline const_iterator end() const;
  inline iterator end();

  // dynamic resizing
  void resize(size_t);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);

  HashMap<K, V> &operator=(const HashMap<K, V> &);

  struct Pair {
    K key;
    V value;
  };

  enum { MAX_NBUCKETS = 4194303,
	 DEFAULT_INITIAL_NBUCKETS = 127,
	 DEFAULT_RESIZE_THRESHOLD = 2 };

 private:

    struct Elt : public Pair {
	Elt *next;
#if defined(__GNUC__) && __GNUC__ < 4
	/* Shut up compiler about Pair lacking default constructor */
	Elt(const Pair &p)		: Pair(p) { }
#endif
    };

  Elt **_buckets;
  size_t _nbuckets;
  V _default_value;

  size_t _n;
  size_t _capacity;

  HashMap_Arena *_arena;

  void initialize(HashMap_ArenaFactory *, size_t);
  void copy_from(const HashMap<K, V> &);
  void resize0(size_t);
  size_t bucket(const K &) const;

  friend class _HashMap_iterator<K, V>;
  friend class _HashMap_const_iterator<K, V>;

};

template <class K, class V>
class _HashMap_const_iterator { public:

  bool live() const			{ return _elt; }
  typedef bool (_HashMap_const_iterator::*unspecified_bool_type)() const;
  inline operator unspecified_bool_type() const CLICK_DEPRECATED;
  void operator++(int);
  void operator++()			{ (*this)++; }

  typedef typename HashMap<K, V>::Pair Pair;
  const Pair *pair() const		{ return _elt; }

  const K &key() const			{ return _elt->key; }
  const V &value() const		{ return _elt->value; }

 private:

  const HashMap<K, V> *_hm;
  typename HashMap<K, V>::Elt *_elt;
  size_t _bucket;

  _HashMap_const_iterator(const HashMap<K, V> *m, bool begin);
  friend class HashMap<K, V>;
  friend class _HashMap_iterator<K, V>;

};

template <class K, class V>
class _HashMap_iterator : public _HashMap_const_iterator<K, V> { public:

  typedef _HashMap_const_iterator<K, V> inherited;

  typedef typename HashMap<K, V>::Pair Pair;
  Pair *pair() const	{ return const_cast<Pair *>(inherited::pair()); }
  V &value() const	{ return const_cast<V &>(inherited::value()); }

 private:

  _HashMap_iterator(HashMap<K, V> *m, bool begin) : inherited(m, begin) { }
  friend class HashMap<K, V>;

};

template <class K, class V>
inline typename HashMap<K, V>::const_iterator
HashMap<K, V>::begin() const
{
  return const_iterator(this, true);
}

template <class K, class V>
inline typename HashMap<K, V>::iterator
HashMap<K, V>::begin()
{
  return iterator(this, true);
}

template <class K, class V>
inline typename HashMap<K, V>::const_iterator
HashMap<K, V>::end() const
{
  return const_iterator(this, false);
}

template <class K, class V>
inline typename HashMap<K, V>::iterator
HashMap<K, V>::end()
{
  return iterator(this, false);
}

template <class K, class V>
inline V *
HashMap<K, V>::findp(const K &key) const
{
  Pair *p = find_pair(key);
  return (p ? &p->value : 0);
}

template <class K, class V>
inline const V &
HashMap<K, V>::find(const K &key, const V &default_value) const
{
  Pair *p = find_pair(key);
  const V *v = (p ? &p->value : &default_value);
  return *v;
}

template <class K, class V>
inline const V &
HashMap<K, V>::find(const K &key) const
{
  return find(key, _default_value);
}

template <class K, class V>
inline const V &
HashMap<K, V>::operator[](const K &key) const
{
  return find(key);
}

template <class K, class V>
inline
_HashMap_const_iterator<K, V>::operator unspecified_bool_type() const
{
    return live() ? &_HashMap_const_iterator::live : 0;
}


template <class K>
class HashMap<K, void *> { public:

    typedef K key_type;
    typedef void *mapped_type;
    struct Pair;

  HashMap();
  explicit HashMap(void *, HashMap_ArenaFactory * = 0);
  HashMap(const HashMap<K, void *> &);
  ~HashMap();

  void set_arena(HashMap_ArenaFactory *);

  size_t size() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  size_t nbuckets() const		{ return _nbuckets; }

  Pair *find_pair(const K &) const;
  inline void **findp(const K &) const;
  inline void *find(const K &, void *) const;
  inline void *find(const K &) const;
  inline void *operator[](const K &) const;

  Pair *find_pair_force(const K &, void *);
  Pair *find_pair_force(const K &k) { return find_pair_force(k, _default_value); }
  void **findp_force(const K &k, void *v) { if (Pair *p = find_pair_force(k, v)) return &p->value; else return 0; }
  void *&find_force(const K &k, void *v) { return *findp_force(k, v); }
  void **findp_force(const K &k) { return findp_force(k, _default_value); }
  void *&find_force(const K &k)  { return *findp_force(k, _default_value); }

  bool insert(const K &, void *);
  bool erase(const K &);
  bool remove(const K &key) {
    return erase(key);
  }
  void clear();

  void swap(HashMap<K, void *> &);

  // iterators
  typedef _HashMap_const_iterator<K, void *> const_iterator;
  typedef _HashMap_iterator<K, void *> iterator;
  inline const_iterator begin() const;
  inline iterator begin();
  inline const_iterator end() const;
  inline iterator end();

  // dynamic resizing
  void resize(size_t);
  bool dynamic_resizing() const		{ return _capacity < 0x7FFFFFFF; }
  void set_dynamic_resizing(bool);

  HashMap<K, void *> &operator=(const HashMap<K, void *> &);

  struct Pair {
    K key;
    void *value;
  };

  enum { MAX_NBUCKETS = 32767,
	 DEFAULT_INITIAL_NBUCKETS = 127,
	 DEFAULT_RESIZE_THRESHOLD = 2 };

  private:

    struct Elt : public Pair {
	Elt *next;
#if defined(__GNUC__) && __GNUC__ < 4
	/* Shut up compiler about Pair lacking default constructor */
	Elt(const Pair &p)		: Pair(p) { }
#endif
    };

  Elt **_buckets;
  size_t _nbuckets;
  void *_default_value;

  size_t _n;
  size_t _capacity;

  HashMap_Arena *_arena;

  void initialize(HashMap_ArenaFactory *, size_t);
  void copy_from(const HashMap<K, void *> &);
  void resize0(size_t);
  size_t bucket(const K &) const;

  friend class _HashMap_iterator<K, void *>;
  friend class _HashMap_const_iterator<K, void *>;

};

template <class K>
class _HashMap_const_iterator<K, void *> { public:

  bool live() const			{ return _elt; }
  typedef bool (_HashMap_const_iterator::*unspecified_bool_type)() const;
  inline operator unspecified_bool_type() const CLICK_DEPRECATED;
  void operator++(int);
  void operator++()			{ (*this)++; }

  typedef typename HashMap<K, void *>::Pair Pair;
  const Pair *pair() const		{ return _elt; }

  const K &key() const			{ return _elt->key; }
  void *value() const			{ return _elt->value; }

 private:

  const HashMap<K, void *> *_hm;
  typename HashMap<K, void *>::Elt *_elt;
  size_t _bucket;

  _HashMap_const_iterator(const HashMap<K, void *> *, bool begin);
  template <class, class> friend class _HashMap_const_iterator;
  template <class, class> friend class _HashMap_iterator;
  template <class, class> friend class HashMap;

};

template <class K>
class _HashMap_iterator<K, void *> : public _HashMap_const_iterator<K, void *> { public:

  typedef _HashMap_const_iterator<K, void *> inherited;

  typedef typename HashMap<K, void *>::Pair Pair;
  Pair *pair() const	{ return const_cast<Pair *>(inherited::pair()); }
  void *&value() const	{ return this->_elt->value; }

 private:

  _HashMap_iterator(HashMap<K, void *> *m, bool begin) : inherited(m, begin) { }
  template <class, class> friend class HashMap;

};

template <class K>
inline typename HashMap<K, void *>::const_iterator
HashMap<K, void *>::begin() const
{
  return const_iterator(this, true);
}

template <class K>
inline typename HashMap<K, void *>::iterator
HashMap<K, void *>::begin()
{
  return iterator(this, true);
}

template <class K>
inline typename HashMap<K, void *>::const_iterator
HashMap<K, void *>::end() const
{
  return const_iterator(this, false);
}

template <class K>
inline typename HashMap<K, void *>::iterator
HashMap<K, void *>::end()
{
  return iterator(this, false);
}

template <class K>
inline void **
HashMap<K, void *>::findp(const K &key) const
{
  Pair *p = find_pair(key);
  return (p ? &p->value : 0);
}

template <class K>
inline void *
HashMap<K, void *>::find(const K &key, void *default_value) const
{
  Pair *p = find_pair(key);
  return (p ? p->value : default_value);
}

template <class K>
inline void *
HashMap<K, void *>::find(const K &key) const
{
  return find(key, _default_value);
}

template <class K>
inline void *
HashMap<K, void *>::operator[](const K &key) const
{
  return find(key);
}

template <class K>
inline
_HashMap_const_iterator<K, void *>::operator unspecified_bool_type() const
{
    return live() ? &_HashMap_const_iterator::live : 0;
}


template <class K, class T>
class HashMap<K, T *> : public HashMap<K, void *> { public:

    typedef K key_type;
    typedef T *mapped_type;
    typedef HashMap<K, void *> inherited;
    struct Pair;

  HashMap()				: inherited() { }
  explicit HashMap(T *def, HashMap_ArenaFactory *factory = 0)
					: inherited(def, factory) { }
  HashMap(const HashMap<K, T *> &o) : inherited(o) { }
  ~HashMap()				{ }

  void set_arena(HashMap_ArenaFactory *af) { inherited::set_arena(af); }

  // size_t size() const		inherited
  // bool empty() const			inherited
  // size_t nbuckets() const		inherited

  Pair *find_pair(const K &k) const { return reinterpret_cast<Pair *>(inherited::find_pair(k)); }
  T **findp(const K &k) const { return reinterpret_cast<T **>(inherited::findp(k)); }
  T *find(const K &k, T *v) const { return reinterpret_cast<T *>(inherited::find(k, v)); }
  T *find(const K &k) const { return reinterpret_cast<T *>(inherited::find(k)); }
  T *operator[](const K &k) const { return reinterpret_cast<T *>(inherited::operator[](k)); }

  Pair *find_pair_force(const K &k, T *v) { return reinterpret_cast<Pair *>(inherited::find_pair_force(k, v)); }
  Pair *find_pair_force(const K &k) { return reinterpret_cast<Pair *>(inherited::find_pair_force(k)); }
  T **findp_force(const K &k, T *v) { return reinterpret_cast<T **>(inherited::findp_force(k, v)); }
  T *&find_force(const K &k, T *v) { return *reinterpret_cast<T **>(inherited::findp_force(k, v)); }
  T **findp_force(const K &k) { return reinterpret_cast<T **>(inherited::findp_force(k)); }
  T *&find_force(const K &k) { return *reinterpret_cast<T **>(inherited::findp_force(k)); }

  bool insert(const K &k, T *v)		{ return inherited::insert(k, v); }
  // bool erase(const K &)		inherited
  // bool remove(const K &)		inherited
  // void clear()			inherited

  void swap(HashMap<K, T *> &o)	{ inherited::swap(o); }

  // iteration
  typedef _HashMap_const_iterator<K, T *> const_iterator;
  typedef _HashMap_iterator<K, T *> iterator;
  inline const_iterator begin() const;
  inline iterator begin();
  inline const_iterator end() const;
  inline iterator end();

  // dynamic resizing methods		inherited

  HashMap<K, T *> &operator=(const HashMap<K, T *> &o) { return static_cast<HashMap<K, T *> &>(inherited::operator=(o)); }

  struct Pair {
    K key;
    T *value;
  };

};

template <class K, class T>
class _HashMap_const_iterator<K, T *> { public:

    typedef _HashMap_const_iterator<K, void *> inherited;

    bool live() const		{ return _i.live(); }
    typedef typename inherited::unspecified_bool_type unspecified_bool_type;
    inline operator unspecified_bool_type() const CLICK_DEPRECATED;
    void operator++(int)	{ _i.operator++(0); }
    void operator++()		{ _i.operator++(); }

    typedef typename HashMap<K, T *>::Pair Pair;
    const Pair *pair() const { return reinterpret_cast<const Pair *>(_i.pair()); }

    const K &key() const	{ return _i.key(); }
    T *value() const		{ return reinterpret_cast<T *>(_i.value()); }

 private:

    inherited _i;

    _HashMap_const_iterator(const HashMap<K, T *> *t, bool begin) : _i(t, begin) { }
    friend class _HashMap_iterator<K, T *>;
    template <class, class> friend class HashMap;

};

template <class K, class T>
class _HashMap_iterator<K, T *> : public _HashMap_const_iterator<K, T *> { public:

  typedef _HashMap_const_iterator<K, T *> inherited;

  typedef typename HashMap<K, T *>::Pair Pair;
  Pair *pair() const	{ return const_cast<Pair *>(inherited::pair()); }
  T *&value() const	{ return pair()->value; }

 private:

  _HashMap_iterator(HashMap<K, T *> *t, bool begin) : inherited(t, begin) { }
  template <class, class> friend class HashMap;

};

template <class K, class T>
inline typename HashMap<K, T *>::const_iterator
HashMap<K, T *>::begin() const
{
  return const_iterator(this, true);
}

template <class K, class T>
inline typename HashMap<K, T *>::iterator
HashMap<K, T *>::begin()
{
  return iterator(this, true);
}

template <class K, class T>
inline typename HashMap<K, T *>::const_iterator
HashMap<K, T *>::end() const
{
  return const_iterator(this, false);
}

template <class K, class T>
inline typename HashMap<K, T *>::iterator
HashMap<K, T *>::end()
{
  return iterator(this, false);
}

template <class K, class T>
inline
_HashMap_const_iterator<K, T *>::operator unspecified_bool_type() const
{
    return inherited::live() ? &inherited::live : 0;
}

template <class K, class V>
inline bool
operator==(const _HashMap_const_iterator<K, V> &a, const _HashMap_const_iterator<K, V> &b)
{
  return a.pair() == b.pair();
}

template <class K, class V>
inline bool
operator!=(const _HashMap_const_iterator<K, V> &a, const _HashMap_const_iterator<K, V> &b)
{
  return a.pair() != b.pair();
}

/** @endcond */
CLICK_ENDDECLS
#include <click/hashmap.cc>
#endif

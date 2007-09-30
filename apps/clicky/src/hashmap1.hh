#ifndef CLICK_HASHMAP1_HH
#define CLICK_HASHMAP1_HH
#include <click/hashmap.hh>
#include <click/bighashmap_arena.hh>
#include <click/pair.hh>
CLICK_DECLS

template <typename T>
class HashMap<T> { public:

    typedef typename T::key_type key_type;
    typedef T value_type;
    
    HashMap();
    explicit HashMap(HashMap_ArenaFactory *);
    HashMap(const HashMap<T> &);
    ~HashMap();
    
    void set_arena(HashMap_ArenaFactory *);
    
    size_t size() const			{ return _n; }
    bool empty() const			{ return _n == 0; }
    size_t nbuckets() const		{ return _nbuckets; }
    
    // iteration
    typedef _HashMap_const_iterator<T> const_iterator;
    typedef _HashMap_iterator<T> iterator;
    inline const_iterator begin() const;
    inline iterator begin();
    inline const_iterator end() const;
    inline iterator end();
    
    // finding
    inline const_iterator find(const key_type &) const;
    inline iterator find(const key_type &);
    inline iterator find_force(const key_type &);
    inline iterator find_force(const value_type &);
    
    bool insert(const value_type &);
    bool remove(const key_type &);
    void clear();
    
    void swap(HashMap<T> &);

    // dynamic resizing
    void resize(size_t);
    bool dynamic_resizing() const	{ return _capacity < 0x7FFFFFFF; }
    void set_dynamic_resizing(bool);
    
    HashMap<T> &operator=(const HashMap<T> &);
    
    enum { MAX_NBUCKETS = 32767,
	   DEFAULT_INITIAL_NBUCKETS = 127,
	   DEFAULT_RESIZE_THRESHOLD = 2 };
    
  private:
    
    struct Elt {
	T v;
	Elt *next;
	Elt(const T &v)			: v(v) { }
    };

    Elt **_buckets;
    size_t _nbuckets;
    
    size_t _n;
    size_t _capacity;
    
    HashMap_Arena *_arena;
    
    void initialize(HashMap_ArenaFactory *, size_t);
    void copy_from(const HashMap<T> &);
    void resize0(size_t);
    size_t bucket(const key_type &) const;
    void insert_new(const value_type &, size_t);
    
    friend class _HashMap_iterator<T>;
    friend class _HashMap_const_iterator<T>;
  
};


/*****
 *
 * iterators
 *
 */

template <typename T>
class _HashMap_const_iterator<T> { public:

    bool live() const			{ return _elt; }
    typedef bool (_HashMap_const_iterator::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;
    void operator++(int);
    void operator++()			{ (*this)++; }

    const T *get() const		{ return &_elt->v; }
    const T *operator->() const		{ return &_elt->v; }
    const T &operator*() const		{ return _elt->v; }

  private:
    
    const HashMap<T> *_m;
    typename HashMap<T>::Elt *_elt;
    size_t _bucket;
    
    inline _HashMap_const_iterator(const HashMap<T> *m, typename HashMap<T>::Elt *elt, size_t bucket)
	: _m(m), _elt(elt), _bucket(bucket) {
    }

    _HashMap_const_iterator(const HashMap<T> *m);
    
    template <typename X>
    inline _HashMap_const_iterator(const _HashMap_const_iterator<X> &other)
	: _m(reinterpret_cast<const HashMap<T> *>(other._m)),
	  _elt(reinterpret_cast<typename HashMap<T>::Elt *>(other._elt)),
	  _bucket(other._bucket)  {
    }
    
    friend class HashMap<T>;
    friend class _HashMap_iterator<T>;

};

template <typename T>
class _HashMap_iterator<T> : public _HashMap_const_iterator<T> { public:
    
    typedef _HashMap_const_iterator<T> inherited;
    
    T *get() const {
	return const_cast<T *>(inherited::get());
    }
    
    inline T *operator->() const {
	return const_cast<T *>(inherited::operator->());
    }
    
    inline T &operator*() const {
	return const_cast<T &>(inherited::operator*());
    }

  private:
    
    inline _HashMap_iterator(HashMap<T> *m, typename HashMap<T>::Elt *elt, size_t bucket)
	: inherited(m, elt, bucket) {
    }

    inline _HashMap_iterator(HashMap<T> *m)
	: inherited(m) {
    }

    template <typename X>
    inline _HashMap_iterator(const _HashMap_const_iterator<X> &other)
	: inherited(other) {
    }
    
    friend class HashMap<T>;

};

template <typename T>
_HashMap_const_iterator<T>::_HashMap_const_iterator(const HashMap<T> *m)
    : _m(m)
{
    size_t nb = _m->_nbuckets;
    typename HashMap<T>::Elt **b = _m->_buckets;
    for (_bucket = 0; _bucket < nb; ++_bucket)
	if (b[_bucket]) {
	    _elt = b[_bucket];
	    return;
	}
    _elt = 0;
}

template <typename T>
inline
_HashMap_const_iterator<T>::operator unspecified_bool_type() const
{
    return live() ? &_HashMap_const_iterator::live : 0;
}

template <typename T>
void
_HashMap_const_iterator<T>::operator++(int)
{
    if (_elt->next)
	_elt = _elt->next;
    else {
	size_t nb = _m->_nbuckets;
	typename HashMap<T>::Elt **b = _m->_buckets;
	for (++_bucket; _bucket < nb; ++_bucket)
	    if (b[_bucket]) {
		_elt = b[_bucket];
		return;
	    }
	_elt = 0;
    }
}

template <typename T>
inline typename HashMap<T>::const_iterator
HashMap<T>::begin() const
{
    return const_iterator(this);
}

template <typename T>
inline typename HashMap<T>::iterator
HashMap<T>::begin()
{
    return iterator(this);
}

template <typename T>
inline typename HashMap<T>::const_iterator
HashMap<T>::end() const
{
    return const_iterator(this, 0, _nbuckets);
}

template <typename T>
inline typename HashMap<T>::iterator
HashMap<T>::end()
{
    return iterator(this, 0, _nbuckets);
}

template <typename T>
inline bool
operator==(const _HashMap_const_iterator<T> &a, const _HashMap_const_iterator<T> &b)
{
  return a.operator->() == b.operator->();
}

template <typename T>
inline bool
operator!=(const _HashMap_const_iterator<T> &a, const _HashMap_const_iterator<T> &b)
{
  return a.operator->() != b.operator->();
}


/*****
 *
 * finding
 *
 */

template <typename T>
_HashMap_const_iterator<T>
HashMap<T>::find(const key_type &key) const
{
    size_t b = bucket(key);
    for (Elt *e = _buckets[b]; e; e = e->next)
	if (hashkey(e->v) == key)
	    return _HashMap_const_iterator<T>(this, e, b);
    return _HashMap_const_iterator<T>(this, 0, _nbuckets);
}

template <typename T>
_HashMap_iterator<T>
HashMap<T>::find(const key_type &key)
{
    size_t b = bucket(key);
    for (Elt *e = _buckets[b]; e; e = e->next)
	if (hashkey(e->v) == key)
	    return _HashMap_iterator<T>(this, e, b);
    return _HashMap_iterator<T>(this, 0, _nbuckets);
}

template <typename T>
_HashMap_iterator<T>
HashMap<T>::find_force(const key_type &key)
{
    size_t b = bucket(key);
    for (Elt *e = _buckets[b]; e; e = e->next)
	if (hashkey(e->v) == key)
	    return _HashMap_iterator<T>(this, e, b);
    if (_n >= _capacity) {
	resize(_nbuckets + 1);
	b = bucket(key);
    }
    if (Elt *e = reinterpret_cast<Elt *>(_arena->alloc())) {
	new(reinterpret_cast<void *>(&e->v)) T(key);
	e->next = _buckets[b];
	_buckets[b] = e;
	_n++;
	return _HashMap_iterator<T>(this, e, b);
    } else
	return _HashMap_iterator<T>(this, 0, _nbuckets);
}

template <typename T>
_HashMap_iterator<T>
HashMap<T>::find_force(const value_type &v)
{
    size_t b = bucket(hashkey(v));
    for (Elt *e = _buckets[b]; e; e = e->next)
	if (hashkey(e->v) == hashkey(v))
	    return _HashMap_iterator<T>(this, e, b);
    if (_n >= _capacity) {
	resize(_nbuckets + 1);
	b = bucket(hashkey(v));
    }
    if (Elt *e = reinterpret_cast<Elt *>(_arena->alloc())) {
	new(reinterpret_cast<void *>(&e->v)) T(v);
	e->next = _buckets[b];
	_buckets[b] = e;
	_n++;
	return _HashMap_iterator<T>(this, e, b);
    } else
	return _HashMap_iterator<T>(this, 0, _nbuckets);
}

template <typename T>
void
HashMap<T>::initialize(HashMap_ArenaFactory *factory, size_t initial_nbuckets)
{
    _nbuckets = initial_nbuckets;
    _buckets = new Elt *[_nbuckets];
    for (size_t i = 0; i < _nbuckets; i++)
	_buckets[i] = 0;
    set_dynamic_resizing(true);
    _n = 0;
    set_arena(factory);
}

template <typename T>
HashMap<T>::HashMap()
    : _arena(0)
{
    initialize(0, DEFAULT_INITIAL_NBUCKETS);
}

template <typename T>
HashMap<T>::HashMap(HashMap_ArenaFactory *factory)
    : _arena(0)
{
    initialize(factory, DEFAULT_INITIAL_NBUCKETS);
}

template <typename T>
void
HashMap<T>::copy_from(const HashMap<T> &o)
  // requires that 'this' is empty and has the same number of buckets as 'o'
  // and the same resize policy
{
    for (size_t i = 0; i < _nbuckets; i++) {
	Elt **pprev = &_buckets[i];
	*pprev = 0;
	for (const Elt *e = o._buckets[i]; e; e = e->next) {
	    Elt *ee = reinterpret_cast<Elt *>(_arena->alloc());
	    new(reinterpret_cast<void *>(&ee->v)) T(e->v);
	    ee->next = 0;
	    *pprev = ee;
	    pprev = &ee->next;
	}
    }
    _n = o._n;
}

template <typename T>
HashMap<T>::HashMap(const HashMap<T> &o)
    : _buckets(new Elt *[o._nbuckets]), _nbuckets(o._nbuckets),
      _capacity(o._capacity), _arena(o._arena)
{
    _arena->use();
    copy_from(o);
}

template <typename T>
HashMap<T> &
HashMap<T>::operator=(const HashMap<T> &o)
{
    if (&o != this) {
	clear();
	if (_nbuckets < o._nbuckets)
	    resize0(o._nbuckets);
	_nbuckets = o._nbuckets;
	_capacity = o._capacity;
	copy_from(o);
    }
    return *this;
}

template <typename T>
HashMap<T>::~HashMap()
{
    for (size_t i = 0; i < _nbuckets; i++)
	for (Elt *e = _buckets[i]; e; ) {
	    Elt *next = e->next;
	    e->v.~T();
	    _arena->free(e);
	    e = next;
	}
    delete[] _buckets;
    _arena->unuse();
}

template <typename T>
void
HashMap<T>::set_dynamic_resizing(bool on)
{
    if (!on)
	_capacity = 0x7FFFFFFF;
    else if (_nbuckets >= MAX_NBUCKETS)
	_capacity = 0x7FFFFFFE;
    else
	_capacity = DEFAULT_RESIZE_THRESHOLD * _nbuckets;
}

template <typename T>
void
HashMap<T>::set_arena(HashMap_ArenaFactory *factory)
{
    assert(empty());
    if (_arena)
	_arena->unuse();
    _arena = HashMap_ArenaFactory::get_arena(sizeof(Elt), factory);
    _arena->use();
}

template <typename T>
inline size_t
HashMap<T>::bucket(const key_type &key) const
{
    return ((size_t) hashcode(key)) % _nbuckets;
}

template <typename T>
void
HashMap<T>::resize0(size_t new_nbuckets)
{
    Elt **new_buckets = new Elt *[new_nbuckets];
    for (size_t i = 0; i < new_nbuckets; i++)
	new_buckets[i] = 0;
    
    size_t old_nbuckets = _nbuckets;
    Elt **old_buckets = _buckets;
    _nbuckets = new_nbuckets;
    _buckets = new_buckets;
    if (dynamic_resizing())
	set_dynamic_resizing(true);	// reset threshold
    
    for (size_t i = 0; i < old_nbuckets; i++)
	for (Elt *e = old_buckets[i]; e; ) {
	    Elt *n = e->next;
	    size_t b = bucket(hashkey(e->v));
	    e->next = new_buckets[b];
	    new_buckets[b] = e;
	    e = n;
	}
    
    delete[] old_buckets;
}

template <typename T>
void
HashMap<T>::resize(size_t want_nbuckets)
{
    size_t new_nbuckets = 1;
    while (new_nbuckets < want_nbuckets && new_nbuckets < MAX_NBUCKETS)
	new_nbuckets = ((new_nbuckets + 1) << 1) - 1;
    assert(new_nbuckets > 0 && new_nbuckets <= MAX_NBUCKETS);
    if (_nbuckets != new_nbuckets)
	resize0(new_nbuckets);
}

template <typename T>
bool
HashMap<T>::insert(const value_type &value)
{
    size_t b = bucket(hashkey(value));
    for (Elt *e = _buckets[b]; e; e = e->next)
	if (hashkey(e->v) == hashkey(value)) {
	    e->v = value;
	    return false;
	}

    if (_n >= _capacity) {
	resize(_nbuckets + 1);
	b = bucket(hashkey(value));
    }
    if (Elt *e = reinterpret_cast<Elt *>(_arena->alloc())) {
	new(reinterpret_cast<void *>(&e->v)) T(value);
	e->next = _buckets[b];
	_buckets[b] = e;
	_n++;
    }
    return true;
}

template <typename T>
bool
HashMap<T>::remove(const key_type &key)
{
    size_t b = bucket(key);
    Elt *prev = 0;
    Elt *e = _buckets[b];
    while (e && !(hashkey(e->v) == key)) {
	prev = e;
	e = e->next;
    }
    if (e) {
	if (prev)
	    prev->next = e->next;
	else
	    _buckets[b] = e->next;
	e->v.~T();
	_arena->free(e);
	_n--;
	return true;
    } else
	return false;
}

template <typename T>
void
HashMap<T>::clear()
{
    for (size_t i = 0; i < _nbuckets; i++) {
	for (Elt *e = _buckets[i]; e; ) {
	    Elt *next = e->next;
	    e->v.~T();
	    _arena->free(e);
	    e = next;
	}
	_buckets[i] = 0;
    }
    _n = 0;
}

template <typename T>
void
HashMap<T>::swap(HashMap<T> &o)
{
    Elt **t_elts;
    size_t t_size;
    HashMap_Arena *t_arena;
    
    t_elts = _buckets; _buckets = o._buckets; o._buckets = t_elts;
    t_size = _nbuckets; _nbuckets = o._nbuckets; o._nbuckets = t_size;
    t_size = _n; _n = o._n; o._n = t_size;
    t_size = _capacity; _capacity = o._capacity; o._capacity = t_size;
    t_arena = _arena; _arena = o._arena; o._arena = t_arena;
}


/*****
 *
 * optimizing specializations
 *
 */

template <typename K, typename V>
class HashMap<Pair<K, V*> > : public HashMap<Pair<K, void*> > { public:

    typedef HashMap<Pair<K, void*> > inherited;
    typedef typename Pair<K, V*>::key_type key_type;
    typedef Pair<K, V*> value_type;
    
    inline HashMap();
    explicit inline HashMap(HashMap_ArenaFactory *);
    inline HashMap(const HashMap<Pair<K, V*> > &);
    
    // iteration
    typedef _HashMap_const_iterator<value_type> const_iterator;
    typedef _HashMap_iterator<value_type> iterator;
    inline const_iterator begin() const;
    inline iterator begin();
    inline const_iterator end() const;
    inline iterator end();
    
    // finding
    inline const_iterator find(const key_type &) const;
    inline iterator find(const key_type &);
    inline iterator find_force(const key_type &);
    inline iterator find_force(const value_type &);
    
    bool insert(const value_type &);
    
    void swap(HashMap<value_type> &);

    HashMap<value_type> &operator=(const HashMap<value_type> &);
  
  private:
    
    struct Elt {
	value_type v;
	Elt *next;
	Elt(const value_type &v)		: v(v) { }
    };
    
    friend class _HashMap_iterator<value_type>;
    friend class _HashMap_const_iterator<value_type>;
  
};

template <typename K, typename V>
inline
HashMap<Pair<K, V*> >::HashMap()
    : inherited()
{
}

template <typename K, typename V>
inline
HashMap<Pair<K, V*> >::HashMap(HashMap_ArenaFactory *arena_factory)
    : inherited(arena_factory)
{
}

template <typename K, typename V>
inline
HashMap<Pair<K, V*> >::HashMap(const HashMap<Pair<K, V*> > &o)
    : inherited(o)
{
}

template <typename K, typename V>
inline _HashMap_const_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::begin() const
{
    return const_iterator(inherited::begin());
}

template <typename K, typename V>
inline _HashMap_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::begin()
{
    return iterator(inherited::begin());
}

template <typename K, typename V>
inline _HashMap_const_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::end() const
{
    return const_iterator(inherited::end());
}

template <typename K, typename V>
inline _HashMap_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::end()
{
    return iterator(inherited::end());
}

template <typename K, typename V>
inline _HashMap_const_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::find(const key_type &k) const
{
    return const_iterator(inherited::find(k));
}

template <typename K, typename V>
inline _HashMap_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::find(const key_type &k)
{
    return iterator(inherited::find(k));
}

template <typename K, typename V>
inline _HashMap_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::find_force(const key_type &k)
{
    return iterator(inherited::find_force(k));
}

template <typename K, typename V>
inline _HashMap_iterator<Pair<K, V*> >
HashMap<Pair<K, V*> >::find_force(const value_type &v)
{
    return iterator(inherited::find_force(reinterpret_cast<const Pair<K, void*> &>(v)));
}

template <typename K, typename V>
inline bool
HashMap<Pair<K, V*> >::insert(const value_type &v)
{
    return inherited::insert(reinterpret_cast<const Pair<K, void*> &>(v));
}

template <typename K, typename V>
inline void
HashMap<Pair<K, V*> >::swap(HashMap<value_type> &other)
{
    inherited::swap(other);
}

template <typename K, typename V>
inline HashMap<Pair<K, V*> > &
HashMap<Pair<K, V*> >::operator=(const HashMap<Pair<K, V*> > &other)
{
    inherited::operator=(other);
    return *this;
}

CLICK_ENDDECLS
#endif

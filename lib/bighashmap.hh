#ifndef BIGHASHMAP_HH
#define BIGHASHMAP_HH

// K AND V REQUIREMENTS:
//
// 		k1 == k2
//		K::K(const K &)
// int		K::hashcode() const
//
//		V::V()		(can be expensive; only used for default value)
// 		V::V(const V &)
// V &		V::operator=(const V &)


template <class K, class V> class BigHashMapIterator;

template <class K, class V>
class BigHashMap { public:

  typedef BigHashMapIterator<K, V> Iterator;
  
  BigHashMap();
  explicit BigHashMap(const V &);
  ~BigHashMap();
  
  int nbuckets() const			{ return _nbuckets; }
  int count() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  
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

  struct Arena {
    static const int SIZE = 128;
    static const int ELT_SIZE = sizeof(Elt) / sizeof(int);
    int _first;
    int _padding;		// pad to 8-byte boundary
    int _x[ELT_SIZE * SIZE];

    Arena();
    void clear();
    Elt *alloc();
    Elt *elt(int i)	{ return reinterpret_cast<Elt *>(_x + ELT_SIZE*i); }
  };

  Elt **_buckets;
  int _nbuckets;
  V _default_v;

  int _n;
  int _capacity;

  Elt *_free;
  int _free_arena;
  Arena **_arenas;
  int _narenas;
  int _arenas_cap;

  void initialize();
  void resize0(int = -1);
  int bucket(const K &) const;
  Elt *find_elt(const K &) const;

  Elt *alloc();
  Elt *slow_alloc();
  void free(Elt *);

  friend class BigHashMapIterator<K, V>;
  
};

template <class K, class V>
class BigHashMapIterator {

  const BigHashMap<K, V> *_hm;
  BigHashMap<K, V>::Elt *_elt;
  int _bucket;

 public:

  BigHashMapIterator(const BigHashMap<K, V> *);

  operator bool() const			{ return _elt; }
  void operator++(int = 0);
  
  const K &key() const			{ return _elt->k; }
  const V &value() const		{ return _elt->v; }
  
};


template <class K, class V>
inline const V &
BigHashMap<K, V>::find(const K &key) const
{
  Elt *e = find_elt(key);
  return e ? e->v : _default_v;
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

#endif

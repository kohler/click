#ifndef BIGHASHMAP_HH
#define BIGHASHMAP_HH

template <class K, class V> class BigHashMapIterator;

template <class K, class V>
class BigHashMap {
  
  struct Elt {
    K k;
    V v;
    Elt *next;
  };

  struct Arena {
    static const int SIZE = 128;
    static const int ELT_SIZE = sizeof(Elt) / sizeof(int);
    int _free;
    int _first;
    int _nalloc;
    int _x[ELT_SIZE * SIZE];

    Arena();
    void clear();
    bool full() const			{ return _free < 0 && _first >= SIZE; }
    bool owns(Elt *e) const;
    bool elt_is_below(Elt *e) const;
    Elt *alloc();
    void free(Elt *e);
  };

  Elt **_buckets;
  int _nbuckets;
  V _default_v;

  int _n;
  int _capacity;

  int _free_arena;
  Arena **_arenas;
  int _narenas;
  int _arenas_cap;

  void initialize();
  void increase(int = -1);
  void check_size();
  int bucket(const K &) const;
  Elt *find_elt(const K &) const;

  Elt *alloc();
  void free(Elt *);

  friend class BigHashMapIterator<K, V>;
  
 public:

  typedef BigHashMapIterator<K, V> Iterator;
  
  BigHashMap();
  explicit BigHashMap(const V &);
  ~BigHashMap();
  
  int count() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  
  const V &find(const K &) const;
  V *findp(const K &) const;
  const V &operator[](const K &k) const;
  V &find_force(const K &);
  
  bool insert(const K &, const V &);
  bool remove(const K &);
  void clear();
  
  Iterator first() const		{ return Iterator(this); }
  
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

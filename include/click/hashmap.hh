#ifndef CLICK_HASHMAP_HH
#define CLICK_HASHMAP_HH

// K AND V REQUIREMENTS:
//
//		K::K()
// 		K::operator bool() const
//			Must have (bool)(K()) == false
//			and no k with (bool)k == false is stored.
// K &		K::operator=(const K &)
// 		k1 == k2
// int		hashcode(const K &)
//			If hashcode(k1) != hashcode(k2), then k1 != k2.
//
//		V::V()
// V &		V::operator=(const V &)

template <class K, class V> class HashMapIterator;

template <class K, class V>
class HashMap { public:

  typedef HashMapIterator<K, V> Iterator;
  
  HashMap();
  explicit HashMap(const V &);
  HashMap(const HashMap<K, V> &);
  ~HashMap()				{ delete[] _e; }

  int size() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  int capacity() const			{ return _capacity; }
  
  const V &find(const K &) const;
  V *findp(const K &) const;
  const V &operator[](const K &k) const;
  V &find_force(const K &);
  
  bool insert(const K &, const V &);
  void clear();
  
  Iterator first() const		{ return Iterator(this); }
  
  HashMap<K, V> &operator=(const HashMap<K, V> &);
  void swap(HashMap<K, V> &);

  void resize(int);
  
 private:
  
  struct Elt { K k; V v; Elt(): k(), v() { } };
  
  int _capacity;
  int _grow_limit;
  int _n;
  Elt *_e;
  V _default_v;
  
  void increase();
  void check_capacity();
  int bucket(const K &) const;

  friend class HashMapIterator<K, V>;
  
};

template <class K, class V>
class HashMapIterator { public:

  HashMapIterator(const HashMap<K, V> *);

  operator bool() const			{ return _pos < _hm->_capacity; }
  void operator++(int);
  
  const K &key() const			{ return _hm->_e[_pos].k; }
  V &value()				{ return _hm->_e[_pos].v; }
  const V &value() const		{ return _hm->_e[_pos].v; }
  
 private:

  const HashMap<K, V> *_hm;
  int _pos;

};


template <class K, class V>
inline const V &
HashMap<K, V>::find(const K &key) const
{
  int i = bucket(key);
  const V *v = (_e[i].k ? &_e[i].v : &_default_v);
  return *v;
}

template <class K, class V>
inline const V &
HashMap<K, V>::operator[](const K &key) const
{
  return find(key);
}

template <class K, class V>
inline V *
HashMap<K, V>::findp(const K &key) const
{
  int i = bucket(key);
  return _e[i].k ? &_e[i].v : 0;
}

inline int
hashcode(int i)
{
  return i;
}

inline unsigned
hashcode(unsigned u)
{
  return u;
}

#endif

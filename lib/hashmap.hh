#ifndef HASHMAP_HH
#define HASHMAP_HH

template <class K, class V> class HashMapIterator;

template <class K, class V>
class HashMap {
  
  struct Elt { K k; V v; };
  
  int _size;
  int _capacity;
  int _n;
  Elt *_e;
  V _default_v;
  
  void increase();
  void check_size();
  int bucket(const K &) const;

  friend class HashMapIterator<K, V>;
  
 public:

  typedef HashMapIterator<K, V> Iterator;
  
  HashMap();
  explicit HashMap(const V &);
  HashMap(const HashMap<K, V> &);
  ~HashMap()				{ delete[] _e; }
  
  int count() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  
  const V &find(const K &) const;
  V *findp(const K &) const;
  const V &operator[](const K &k) const;
  V &find_force(const K &);
  
  bool insert(const K &, const V &);
  void clear();
  int size() const			{ return _size; }
  void set_size(int i);
  
  Iterator first() const		{ return Iterator(this); }
  
  HashMap<K, V> &operator=(const HashMap<K, V> &);
  void swap(HashMap<K, V> &);
  
};

template <class K, class V>
class HashMapIterator {

  const HashMap<K, V> *_hm;
  int _pos;

 public:

  HashMapIterator(const HashMap<K, V> *);

  operator bool() const			{ return _pos < _hm->_size; }
  void operator++(int = 0);
  
  const K &key() const			{ return _hm->_e[_pos].k; }
  const V &value() const		{ return _hm->_e[_pos].v; }
  
};


template <class K, class V>
inline const V &
HashMap<K, V>::find(const K &key) const
{
  int i = bucket(key);
  return _e[i].k ? _e[i].v : _default_v;
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

#endif

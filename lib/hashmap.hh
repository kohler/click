#ifndef HASHMAP_HH
#define HASHMAP_HH

template <class K, class V>
class HashMap {
  
  struct HashMapElt { K k; V v; };
  
  int _size;
  int _capacity;
  int _n;
  HashMapElt *_e;
  V _default_v;
  
  void increase();
  void check_size();
  int bucket(K) const;
  
 public:
  
  HashMap();
  explicit HashMap(const V &);
  HashMap(const HashMap<K, V> &);
  ~HashMap()				{ delete[] _e; }
  
  int count() const			{ return _n; }
  bool empty() const			{ return _n == 0; }
  
  const V &find(K) const;
  V *findp(K) const;
  const V &operator[](K k) const	{ return find(k); }
  V &find_force(K);
  
  bool insert(K, const V &);
  void clear();
  int size() const			{ return _size; }
  void set_size(int i);
  
  bool each(int &, K &, V &) const;
  bool eachp(int &, K *&, V *&) const;
  
  HashMap<K, V> &operator=(const HashMap<K, V> &);
  void swap(HashMap<K, V> &);
  
};


template <class K, class V>
inline const V &
HashMap<K, V>::find(K key) const
{
  int i = bucket(key);
  return _e[i].k ? _e[i].v : _default_v;
}

template <class K, class V>
inline V *
HashMap<K, V>::findp(K key) const
{
  int i = bucket(key);
  return _e[i].k ? &_e[i].v : 0;
}

#endif


#ifndef HASHMAP2_HH
#define HASHMAP2_HH

// requirements on K
//
//  * k1 == k2 (must exist)
//  * int K::hashcode() const
//  * K & K::operator=(const K &)
//  * V & V::operator=(const V &)
//  *     V::V()
//
// - offers insert, find, findp, erase, each, and eachp table operations
// - offers set_hashsize operation that changes the size of the hash space
// - collision list of each hash bucket is implemented using a linked list
// - never explicitly increase size of hashtable

template <class K, class V>
class HashMap2 {
  
  struct HashMap2Elt { 
    K k; 
    V v; 
    struct HashMap2Elt *next_k;
    struct HashMap2Elt *next_i;
  };
  void free_elt (struct HashMap2Elt *e)	{ delete e; }
  struct HashMap2Elt *new_elt()		{ return new struct HashMap2Elt; }

  unsigned _hashsize;
  unsigned _n;
  int hf(K k) const { return (k.hashcode() >> 2) & (_hashsize-1); }

  HashMap2Elt **_k;	// bucket list
  HashMap2Elt *_l;	// list of all elements
  HashMap2Elt *_i;	// iterator
  V _default_v;
  
  void* elt(K) const;
  void* prev_k(K) const;
  void* prev_i(K) const;
  void* elt_or_prev(K) const;

  void destroy();
  
 public:
  
  HashMap2();
  explicit HashMap2(const V &);
  ~HashMap2()				{ destroy(); }
  
  unsigned count() const		{ return _n; }
  bool empty() const			{ return _n == 0; }
  unsigned hashsize() const		{ return _hashsize; }
 
  // sets hash size of table
  void set_hashsize(unsigned i);
  void clear()				{ destroy(); set_hashsize(8); }

  // returns true if pair inserted is new, returns false if it's a replacement
  bool insert(K, const V &);
  // erase K. returns true if erase. returns false if entry does not exist.
  bool erase(K);

  // find V associated with K, returns default value if K not found
  const V &find(K) const;
  const V &operator[](K k) const	{ return find(k); }
  // find pointer to V associated with K. returns 0L if K not found
  V *findp(K) const;

  // iterator. returns true if not done. false otherwise, and iterator resets.
  bool each(K &, V &);
  // iterator. returns true if not done. false otherwise, and iterator resets.
  bool eachp(K *&, V *&);
};


template <class K, class V>
inline const V &
HashMap2<K, V>::find(K key) const
{
  struct HashMap2Elt *e = (struct HashMap2Elt*) elt(key);
  if (e) 
    return e->v;
  else
    return _default_v;
}

template <class K, class V>
inline V *
HashMap2<K, V>::findp(K key) const
{
  struct HashMap2Elt *e = (struct HashMap2Elt*) elt(key);
  if (e)
    return &e->v;
  else
    return 0;
}

#endif


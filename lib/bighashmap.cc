/*
 * bighashmap.{cc,hh} -- a hash table template that supports removal
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include "bighashmap.hh"

// 		k1 == k2
//		K::K(const K &)
// int		K::hashcode() const

// 		V::V(const V &)
// V &		V::operator=(const V &)


template<class K, class V>
inline
BigHashMap<K, V>::Arena::Arena()
{
  _free = -1;
  _first = _nalloc = 0;
}

template<class K, class V>
inline void
BigHashMap<K, V>::Arena::clear()
{
  _free = -1;
  _first = _nalloc = 0;
}

template<class K, class V>
inline bool
BigHashMap<K, V>::Arena::owns(Elt *e) const
{
  int *x = reinterpret_cast<int *>(e);
  return x >= _x && x < _x + ELT_SIZE * SIZE;
}

template<class K, class V>
inline bool
BigHashMap<K, V>::Arena::elt_is_below(Elt *e) const
{
  int *x = reinterpret_cast<int *>(e);
  return x < _x;
}

template<class K, class V>
BigHashMap<K, V>::Elt *
BigHashMap<K, V>::Arena::alloc()
{
  if (_free >= 0) {
    Elt *e = reinterpret_cast<Elt *>(&_x[ELT_SIZE * _free]);
    _free = _x[ELT_SIZE * _free];
    _nalloc++;
    return e;
  } else if (_first < SIZE) {
    Elt *e = reinterpret_cast<Elt *>(&_x[ELT_SIZE * _first]);
    _first++;
    _nalloc++;
    return e;
  } else
    return 0;
}

template<class K, class V>
inline void
BigHashMap<K, V>::Arena::free(Elt *e)
{
  int *x = reinterpret_cast<int *>(e);
  int which = (x - _x) / ELT_SIZE;
  *x = _free;
  _free = which;
  _nalloc--;
}



template <class K, class V>
void
BigHashMap<K, V>::initialize()
{
  _buckets = new Elt *[128];
  _nbuckets = 128;
  for (int i = 0; i < _nbuckets; i++)
    _buckets[i] = 0;
  _capacity = _nbuckets * 3;

  _n = 0;

  _free_arena = -1;
  _arenas = new Arena *[4];
  _narenas = 0;
  _arenas_cap = 4;
}

template <class K, class V>
BigHashMap<K, V>::BigHashMap()
{
  initialize();
}

template <class K, class V>
BigHashMap<K, V>::BigHashMap(const V &def)
  : _default_v(def)
{
  initialize();
}

template <class K, class V>
BigHashMap<K, V>::~BigHashMap()
{
  for (int i = 0; i < _nbuckets; i++)
    for (Elt *e = _buckets[i]; e; e = e->next) {
      e->k.~K();
      e->v.~V();
    }
  delete[] _buckets;
  for (int i = 0; i < _narenas; i++)
    delete _arenas[i];
  delete[] _arenas;
}


template <class K, class V>
BigHashMap<K, V>::Elt *
BigHashMap<K, V>::alloc()
{
  // XXX malloc failures

  int tries = 0;
  while (_free_arena >= 0 && tries < _narenas) {
    Elt *e = _arenas[_free_arena]->alloc();
    if (e) return e;
    _free_arena++;
    if (_free_arena == _narenas) _free_arena = 0;
    tries++;
  }
  
  // add an Arena
  if (_narenas >= _arenas_cap) {
    _arenas_cap *= 2;
    Arena **new_arenas = new Arena *[_arenas_cap];
    memcpy(new_arenas, _arenas, sizeof(Arena *) * _narenas);
    delete[] _arenas;
    _arenas = new_arenas;
  }

  Arena *new_arena = new Arena;
  int pos = 0;
  while (pos < _narenas && new_arena > _arenas[pos])
    pos++;
  memmove(_arenas + pos + 1, _arenas + pos, sizeof(Arena *) * (_narenas - pos));
  _arenas[pos] = new_arena;
  _free_arena = pos;
  return new_arena->alloc();
}

template <class K, class V>
void
BigHashMap<K, V>::free(Elt *e)
{
  e->k.~K();
  e->v.~V();
  if (_free_arena >= 0 && _arenas[_free_arena]->owns(e))
    _arenas[_free_arena]->free(e);
  else {
    int l = 0;
    int r = _narenas - 1;
    while (l <= r) {
      int m = (l + r) >> 1;
      if (_arenas[m]->owns(e)) {
	_arenas[m]->free(e);
	_free_arena = m;
	return;
      } else if (_arenas[m]->elt_is_below(e))
	r = m - 1;
      else
	l = m + 1;
    }
    assert(0);
  }
}


template <class K, class V>
inline int
BigHashMap<K, V>::bucket(const K &key) const
{
  return (key.hashcode() >> 2) & (_nbuckets - 1);
}

template <class K, class V>
BigHashMap<K, V>::Elt *
BigHashMap<K, V>::find_elt(const K &key) const
{
  for (Elt *e = _buckets[bucket(key)]; e; e = e->next)
    if (e->k == key)
      return e;
  return 0;
}


template <class K, class V>
void
BigHashMap<K, V>::increase(int new_nbuckets = -1)
{
  if (new_nbuckets < 0)
    new_nbuckets = _nbuckets * 2;
  Elt **new_buckets = new Elt *[new_nbuckets];
  for (int i = 0; i < new_nbuckets; i++)
    new_buckets[i] = 0;

  int old_nbuckets = _nbuckets;
  Elt **old_buckets = _buckets;
  _nbuckets = new_nbuckets;
  _buckets = new_buckets;
  _capacity = new_nbuckets * 3;
  
  for (int i = 0; i < old_nbuckets; i++)
    for (Elt *e = old_buckets[i]; e; ) {
      Elt *n = e->next;
      int b = bucket(e->k);
      e->next = new_buckets[b];
      new_buckets[b] = e;
      e = n;
    }

  delete[] old_buckets;
}

template <class K, class V>
inline void
BigHashMap<K, V>::check_size()
{
  //if (_n >= _capacity) increase();
}

template <class K, class V>
bool
BigHashMap<K, V>::insert(const K &key, const V &value)
{
  int b = bucket(key);
  for (Elt *e = _buckets[b]; e; e = e->next)
    if (e->k == key) {
      e->v = value;
      return false;
    }

  if (_n >= _capacity) {
    increase();
    b = bucket(key);
  }
  Elt *e = alloc();
  new(reinterpret_cast<void *>(&e->k)) K(key);
  new(reinterpret_cast<void *>(&e->v)) V(value);
  e->next = _buckets[b];
  _buckets[b] = e;
  _n++;
  return true;
}

template <class K, class V>
bool
BigHashMap<K, V>::remove(const K &key)
{
  int b = bucket(key);
  Elt *prev = 0;
  Elt *e = _buckets[b];
  while (e && !(e->k == key)) {
    prev = e;
    e = e->next;
  }
  if (e) {
    if (prev)
      prev->next = e->next;
    else
      _buckets[b] = e->next;
    free(e);
    return true;
  } else
    return false;
}

template <class K, class V>
V &
BigHashMap<K, V>::find_force(const K &key)
{
  int b = bucket(key);
  for (Elt *e = _buckets[b]; e; e = e->next)
    if (e->k == key)
      return _e[i].v;
  Elt *e = alloc();
  new(reinterpret_cast<void *>(&e->k)) K(key);
  new(reinterpret_cast<void *>(&e->v)) V(_default_v);
  e->next = _buckets[b];
  _buckets[b] = e;
  _n++;
  return _e[i].v;
}

template <class K, class V> void
BigHashMap<K, V>::clear()
{
  for (int i = 0; i < _nbuckets; i++) {
    for (Elt *e = _buckets[i]; e; e = e->_next) {
      e->k.~K();
      e->v.~V();
    }
    _buckets[i] = 0;
  }
  _n = 0;
  for (int i = 0; i < _narenas; i++)
    _arenas[i]->clear();
  _free_arena = (_narenas > 0 ? 0 : -1);
}

template <class K, class V>
BigHashMapIterator<K, V>::BigHashMapIterator(const BigHashMap<K, V> *hm)
  : _hm(hm)
{
  int nb = _hm->_nbuckets;
  BigHashMap<K, V>::Elt **b = _hm->_buckets;
  for (_bucket = 0; _bucket < nb; _bucket++)
    if (b[_bucket]) {
      _elt = b[_bucket];
      return;
    }
  _elt = 0;
}

template <class K, class V>
void
BigHashMapIterator<K, V>::operator++(int = 0)
{
  if (_elt->next)
    _elt = _elt->next;
  else {
    int nb = _hm->_nbuckets;
    BigHashMap<K, V>::Elt **b = _hm->_buckets;
    for (_bucket++; _bucket < nb; _bucket++)
      if (b[_bucket]) {
	_elt = b[_bucket];
	return;
      }
    _elt = 0;
  }
}

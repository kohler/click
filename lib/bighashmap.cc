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

#ifdef HAVE_NEW_H
# include <new.h>
#elif !HAVE_PLACEMENT_NEW
inline void *operator new(size_t, void *v) { return v; }
# define HAVE_PLACEMENT_NEW 1
#endif

template<class K, class V>
inline
BigHashMap<K, V>::Arena::Arena()
{
  _first = 0;
}

template<class K, class V>
inline void
BigHashMap<K, V>::Arena::clear()
{
  _first = 0;
}

template<class K, class V>
BigHashMap<K, V>::Elt *
BigHashMap<K, V>::Arena::alloc()
{
  if (_first < SIZE)
    return elt(_first++);
  else
    return 0;
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

  _free = 0;
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
void
BigHashMap<K, V>::set_dynamic_resizing(bool on)
{
  _capacity = (on ? 3 * _nbuckets : 0x7FFFFFFF);
}

template <class K, class V>
BigHashMap<K, V>::Elt *
BigHashMap<K, V>::slow_alloc()
{
  // XXX malloc failures
  if (_free_arena >= 0) {
    Elt *e = _arenas[_free_arena]->alloc();
    if (e)
      return e;
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
  _arenas[_narenas++] = new_arena;
  _free_arena = _narenas - 1;
  return new_arena->alloc();
}

template <class K, class V>
inline BigHashMap<K, V>::Elt *
BigHashMap<K, V>::alloc()
{
  if (_free) {
    Elt *e = _free;
    _free = e->next;
    return e;
  } else
    return slow_alloc();
}

template <class K, class V>
inline void
BigHashMap<K, V>::free(Elt *e)
{
  e->next = _free;
  _free = e;
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
BigHashMap<K, V>::resize0(int new_nbuckets)
{
  Elt **new_buckets = new Elt *[new_nbuckets];
  for (int i = 0; i < new_nbuckets; i++)
    new_buckets[i] = 0;

  int old_nbuckets = _nbuckets;
  Elt **old_buckets = _buckets;
  _nbuckets = new_nbuckets;
  _buckets = new_buckets;
  if (dynamic_resizing())
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
void
BigHashMap<K, V>::resize(int want_nbuckets)
{
  int new_nbuckets = 1;
  while (new_nbuckets < want_nbuckets && new_nbuckets > 0)
    new_nbuckets <<= 1;
  if (new_nbuckets > 0)
    resize0(new_nbuckets);
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
    resize0(_nbuckets << 1);
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
    e->k.~K();
    e->v.~V();
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
      return e->v;
  Elt *e = alloc();
  new(reinterpret_cast<void *>(&e->k)) K(key);
  new(reinterpret_cast<void *>(&e->v)) V(_default_v);
  e->next = _buckets[b];
  _buckets[b] = e;
  _n++;
  return e->v;
}

template <class K, class V>
void
BigHashMap<K, V>::clear()
{
  for (int i = 0; i < _nbuckets; i++) {
    for (Elt *e = _buckets[i]; e; e = e->next) {
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
void
BigHashMap<K, V>::swap(BigHashMap<K, V> &o)
{
  Elt **t_elts, *t_elt;
  V t_v;
  int t_int;
  Arena **t_arenas;

  t_elts = _buckets; _buckets = o._buckets; o._buckets = t_elts;
  t_int = _nbuckets; _nbuckets = o._nbuckets; o._nbuckets = t_int;
  t_v = _default_v; _default_v = o._default_v; o._default_v = t_v;

  t_int = _n; _n = o._n; o._n = t_int;
  t_int = _capacity; _n = o._capacity; o._capacity = t_int;

  t_elt = _free; _free = o._free; o._free = t_elt;
  t_int = _free_arena; _free_arena = o._free_arena; o._free_arena = t_int;
  t_arenas = _arenas; _arenas = o._arenas; o._arenas = t_arenas;
  t_int = _narenas; _narenas = o._narenas; o._narenas = t_int;
  t_int = _arenas_cap; _arenas_cap = o._arenas_cap; o._arenas_cap = t_int;
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

#if 0
static int
BigHashMap_partition_elts(void **elts, int left, int right)
{
  void *pivot = elts[(left + right) / 2];

  // loop invariant:
  // elts[i] < pivot for all left_init <= i < left
  // elts[i] > pivot for all right < i <= right_init
  while (left < right) {
    if (elts[left] < pivot)
      left++;
    else if (elts[right] > pivot)
      right--;
    else {
      void *x = elts[left];
      elts[left] = elts[right];
      elts[right] = x;
    }
  }

  return left;
}

void
BigHashMap_qsort_elts(void **elts, int left, int right)
{
  if (left < right) {
    int split = BigHashMap_partition_elts(elts, left, right);
    BigHashMap_qsort_elts(elts, left, split);
    BigHashMap_qsort_elts(elts, split, right);
  }
}
#endif

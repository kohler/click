/*
 * hashmap.{cc,hh} -- a simple, stupid hash table template
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/hashmap.hh>

template <class K, class V>
HashMap<K, V>::HashMap()
  : _capacity(0), _grow_limit(0), _n(0), _e(0), _default_v()
{
  increase();
}

template <class K, class V>
HashMap<K, V>::HashMap(const V &def)
  : _capacity(0), _grow_limit(0), _n(0), _e(0), _default_v(def)
{
  increase();
}


template <class K, class V>
HashMap<K, V>::HashMap(const HashMap<K, V> &m)
  : _capacity(m._capacity), _grow_limit(m._grow_limit), _n(m._n),
    _e(new Elt[m._capacity]), _default_v(m._default_v)
{
  for (int i = 0; i < _capacity; i++)
    _e[i] = m._e[i];
}


template <class K, class V>
HashMap<K, V> &
HashMap<K, V>::operator=(const HashMap<K, V> &o)
{
  // This works with self-assignment.
  
  _capacity = o._capacity;
  _grow_limit = o._grow_limit;
  _n = o._n;
  _default_v = o._default_v;
  
  Elt *new_e = new Elt[_capacity];
  for (int i = 0; i < _capacity; i++)
    new_e[i] = o._e[i];
  
  delete[] _e;
  _e = new_e;
  
  return *this;
}

template <class K, class V>
inline void
HashMap<K, V>::resize(int i)
{
  while (i > _capacity) increase();
}

template <class K, class V>
inline int
HashMap<K, V>::bucket(const K &key) const
{
  int hc = hashcode(key);
  int i =   hc       & (_capacity - 1);
  int j = ((hc >> 6) & (_capacity - 1)) | 1;
  
  while (_e[i].k && !(_e[i].k == key))
    i = (i + j) & (_capacity - 1);
  
  return i;
}


template <class K, class V>
void
HashMap<K, V>::increase()
{
  Elt *oe = _e;
  int ocap = _capacity;
  
  _capacity *= 2;
  if (_capacity < 8) _capacity = 8;
  _grow_limit = (int)(0.8 * _capacity) - 1;
  _e = new Elt[_capacity];
  
  Elt *otrav = oe;
  for (int i = 0; i < ocap; i++, otrav++)
    if (otrav->k) {
      int j = bucket(otrav->k);
      _e[j] = *otrav;
    }
  
  delete[] oe;
}

template <class K, class V>
inline void
HashMap<K, V>::check_capacity()
{
  if (_n >= _grow_limit) increase();
}

template <class K, class V>
bool
HashMap<K, V>::insert(const K &key, const V &val)
{
  check_capacity();
  int i = bucket(key);
  bool is_new = !(bool)_e[i].k;
  _e[i].k = key;
  _e[i].v = val;
  _n += is_new;
  return is_new;
}

template <class K, class V>
V &
HashMap<K, V>::find_force(const K &key)
{
  check_capacity();
  int i = bucket(key);
  if (!(bool)_e[i].k) {
    _e[i].k = key;
    _e[i].v = _default_v;
    _n++;
  }
  return _e[i].v;
}

template <class K, class V>
void
HashMap<K, V>::clear()
{
  delete[] _e;
  _e = 0;
  _capacity = _grow_limit = _n = 0;
  increase();
}

template <class K, class V>
void
HashMap<K, V>::swap(HashMap<K, V> &o)
{
  int capacity = _capacity;
  int grow_limit = _grow_limit;
  int n = _n;
  Elt *e = _e;
  V default_v = _default_v;
  _capacity = o._capacity;
  _grow_limit = o._grow_limit;
  _n = o._n;
  _e = o._e;
  _default_v = o._default_v;
  o._capacity = capacity;
  o._grow_limit = grow_limit;
  o._n = n;
  o._e = e;
  o._default_v = default_v;
}

template <class K, class V>
HashMapIterator<K, V>::HashMapIterator(const HashMap<K, V> *hm)
  : _hm(hm)
{
  HashMap<K, V>::Elt *e = _hm->_e;
  int capacity = _hm->_capacity;
  for (_pos = 0; _pos < capacity && !(bool)e[_pos].k; _pos++)
    ;
}

template <class K, class V>
void
HashMapIterator<K, V>::operator++(int = 0)
{
  HashMap<K, V>::Elt *e = _hm->_e;
  int capacity = _hm->_capacity;
  for (_pos++; _pos < capacity && !(bool)e[_pos].k; _pos++)
    ;
}

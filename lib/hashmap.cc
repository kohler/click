/*
 * hashmap.{cc,hh} -- a simple, stupid hash table template
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include "hashmap.hh"

// 		k1 == k2  (must exist)
//		K::K()
// 		K::operator bool() const
//			It must be true that (bool)(K()) == 0
//			and that no k with (bool)k == 0 is stored.
// K &		K::operator=(const K &)
// int		K::hashcode() const

// V &		V::operator=(const V &)

template <class K, class V>
HashMap<K, V>::HashMap()
  : _size(0), _capacity(0), _n(0), _e(0)
{
  increase();
}

template <class K, class V>
HashMap<K, V>::HashMap(const V &def)
  : _size(0), _capacity(0), _n(0), _e(0), _default_v(def)
{
  increase();
}


template <class K, class V>
HashMap<K, V>::HashMap(const HashMap<K, V> &m)
  : _size(m._size), _capacity(m._capacity), _n(m._n),
    _e(new Elt[m._size]), _default_v(m._default_v)
{
  for (int i = 0; i < _size; i++)
    _e[i] = m._e[i];
}


template <class K, class V>
HashMap<K, V> &
HashMap<K, V>::operator=(const HashMap<K, V> &o)
{
  // This works with self-assignment.
  
  _size = o._size;
  _capacity = o._capacity;
  _n = o._n;
  _default_v = o._default_v;
  
  Elt *new_e = new Elt[_size];
  for (int i = 0; i < _size; i++)
    new_e[i] = o._e[i];
  
  delete[] _e;
  _e = new_e;
  
  return *this;
}

template <class K, class V>
inline void
HashMap<K, V>::set_size(int i)
{
  while (i > _size) increase();
}

template <class K, class V>
inline int
HashMap<K, V>::bucket(const K &key) const
{
  int hc = key.hashcode();
  int i =  (hc >> 2) & (_size - 1);
  int j = ((hc >> 6) & (_size - 1)) | 1;
  
  while (_e[i].k && !(_e[i].k == key))
    i = (i + j) & (_size - 1);
  
  return i;
}


template <class K, class V>
void
HashMap<K, V>::increase()
{
  Elt *oe = _e;
  int osize = _size;
  
  _size *= 2;
  if (_size < 8) _size = 8;
  _capacity = (int)(0.8 * _size) - 1;
  _e = new Elt[_size];
  
  Elt *otrav = oe;
  for (int i = 0; i < osize; i++, otrav++)
    if (otrav->k) {
      int j = bucket(otrav->k);
      _e[j] = *otrav;
    }
  
  delete[] oe;
}

template <class K, class V>
inline void
HashMap<K, V>::check_size()
{
  if (_n >= _capacity) increase();
}

template <class K, class V>
bool
HashMap<K, V>::insert(const K &key, const V &val)
{
  check_size();
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
  check_size();
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
  _size = _capacity = _n = 0;
  increase();
}

template <class K, class V>
void
HashMap<K, V>::swap(HashMap<K, V> &o)
{
  int size = _size;
  int capacity = _capacity;
  int n = _n;
  Elt *e = _e;
  V default_v = _default_v;
  _size = o._size;
  _capacity = o._capacity;
  _n = o._n;
  _e = o._e;
  _default_v = o._default_v;
  o._size = size;
  o._capacity = capacity;
  o._n = n;
  o._e = e;
  o._default_v = default_v;
}

template <class K, class V>
HashMapIterator<K, V>::HashMapIterator(const HashMap<K, V> *hm)
  : _hm(hm)
{
  HashMap<K, V>::Elt *e = _hm->_e;
  int size = _hm->_size;
  for (_pos = 0; _pos < size && !(bool)e[_pos].k; _pos++)
    ;
}

template <class K, class V>
void
HashMapIterator<K, V>::operator++(int = 0)
{
  HashMap<K, V>::Elt *e = _hm->_e;
  int size = _hm->_size;
  for (_pos++; _pos < size && !(bool)e[_pos].k; _pos++)
    ;
}

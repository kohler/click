/*
 * hashmap2.{cc,hh} -- another hash table template
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include "hashmap2.hh"

template <class K, class V>
HashMap2<K, V>::HashMap2()
  : _hashsize(0), _n(0), _k(0), _l(0)
{
  set_hashsize(8);
}

template <class K, class V>
HashMap2<K, V>::HashMap2(const V &def)
  : _hashsize(0), _n(0), _k(0), _l(0), _default_v(def)
{
  set_hashsize(8);
}

template <class K, class V> inline void
HashMap2<K, V>::destroy()
{
  struct HashMap2Elt *i = _l;
  while(i) {
    struct HashMap2Elt *n = i->next_i;
    free_elt(i);
    i = n;
  }
  _l = 0;
  _i = 0;
  if (_k) {
    delete _k;
    _k = 0;
    _hashsize = 0;
    _n = 0;
  }
}

template <class K, class V> inline void
HashMap2<K, V>::set_hashsize(unsigned i)
{
  if (_k) {
    delete _k;
    _k = 0;
  }
  _hashsize = i;
  
  if (i == 0) {
    destroy();
    return;
  }

  _k = new (HashMap2Elt *)[_hashsize];
  for (unsigned j = 0; j < _hashsize; j++) 
    _k[j] = 0L;

  struct HashMap2Elt *iter = _l;
  while(iter) {
    iter->next_k = 0L;
    struct HashMap2Elt *p = (struct HashMap2Elt *)elt_or_prev(iter->k);

    if (p) 		// insert into bucket
      p->next_k = iter;
    else { 		// bucket empty
      int j = hf(iter->k);
      _k[j] = iter;
    }
    iter = iter->next_i;
  }
  _i = _l;
}

// returns elt with K. returns 0L if does not exist.
template <class K, class V> inline void *
HashMap2<K, V>::elt(K key) const
{
  int i = hf(key);
  struct HashMap2Elt *e = _k[i];
  
  while (e && !(e->k == key))
    e = e->next_k;
  return e;
}

// returns elt with K. returns prev elt if does not exist.
template <class K, class V> inline void *
HashMap2<K, V>::elt_or_prev(K key) const
{
  int i = hf(key);
  struct HashMap2Elt *e = _k[i];
  struct HashMap2Elt *p = 0L;
  
  while (e && !(e->k == key)) {
    p = e;
    e = e->next_k;
  }
  return e ? e : p;
}

// returns elt prev to elt with K. if only K exists in bucket, return that elt
// anyways.
template <class K, class V> inline void *
HashMap2<K, V>::prev_k(K key) const
{
  int i = hf(key);
  struct HashMap2Elt *e = _k[i];
  struct HashMap2Elt *p = _k[i];
  
  while (e && !(e->k == key)) {
    p = e;
    e = e->next_k;
  }
  return p;
}

// returns elt prev to elt with K in _l. if K is in the first elt in _l,
// returns _l.
template <class K, class V> inline void *
HashMap2<K, V>::prev_i(K key) const
{
  struct HashMap2Elt *e = _l;
  struct HashMap2Elt *p = _l;
  
  while (e && !(e->k == key)) {
    p = e;
    e = e->next_i;
  }
  return p;
}

template <class K, class V> bool
HashMap2<K, V>::insert(K key, const V &val)
{
  struct HashMap2Elt *ep = (struct HashMap2Elt *)elt_or_prev(key);
  
  if (ep && ep->k == key) { // replacement
    ep->v = val;
    return false;
  } 
  
  else { // new element
    struct HashMap2Elt *e = new_elt();
    e->k = key;
    e->v = val;
    e->next_k = 0L;
    if (!ep) {
      int i = hf(key);
      _k[i] = e;
    }
    else
      ep->next_k = e;
    e->next_i = _l;
    _l = e;
    _n++;
    return false;
  }
}

template <class K, class V> bool
HashMap2<K, V>::erase(K key)
{
  struct HashMap2Elt *p = (struct HashMap2Elt *)prev_k(key);
  struct HashMap2Elt *e;

  if (!p || (!(p->k == key) && (!(p->next_k) || !(p->next_k->k == key))))
    return false;
 
  if (p->k == key) { // first in bucket
    int i = hf(key);
    _k[i] = p->next_k;
    e = p;
  } else {
    e = p->next_k;
    p->next_k = e->next_k;
  }
  
  p = (struct HashMap2Elt *)prev_i(key);
  if (p->k == key) // first in list
    _l = p->next_i;
  else {
    struct HashMap2Elt *ie = p->next_i;
    p->next_i = ie->next_i;
  }

  _n--;
  free_elt(e);
  return true;
}

template <class K, class V> bool
HashMap2<K, V>::each(K &k, V &v)
{
  if (!_i) {
    _i = _l;
    return false;
  }

  struct HashMap2Elt *e = _i;
  _i = _i->next_i;

  k = e->k;
  v = e->v;
  return true;
}

template <class K, class V> bool
HashMap2<K, V>::eachp(K *&kp, V *&vp)
{
  if (!_i) {
    _i = _l;
    return false;
  }

  struct HashMap2Elt *e = _i;
  _i = _i->next_i;

  kp = &(e->k);
  vp = &(e->v);
  return true;
}


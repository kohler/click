/*
 * radix.{cc,hh} -- a radix tree. Implementation taken from Sedgewick.
 * Thomer M. Gil
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include "radix.hh"


Radix::Radix()
{
  root = new RadixNode;
  root->info = root->key = 0;
  root->bit_idx = KEYSIZE-1;
  root->valid = false;
  root->left = root->right = root;
}

void
Radix::RadixNode::kill()
{
  if (bit_idx >= 0) {
    bit_idx = -1;
    if (left) left->kill();
    if (right) right->kill();
    delete this;
  }
}

Radix::~Radix()
{
  root->kill();
}

void
Radix::insert(KEYTYPE v, INFOTYPE info)
{
  RadixNode *t, *p;
  int i;

  t = node_lookup(v);

  if(v == t->key)
    return;

  // Find bit for which v and t-> key differ
  i = KEYSIZE-1;
  while(bits(v, i, 1) == bits(t->key, i, 1))
    i--;

  // Descend to that level
  RadixNode *x = root;
  do {
    p = x;
    if(bits(v, x->bit_idx, 1) == 0)
      x = x->left;
    else
      x = x->right;

    if((x->bit_idx <= i) || (p->bit_idx <= x->bit_idx))
      break;
  } while(1);

  // ... and instert node
  t = new RadixNode;
  t->key = v;
  t->bit_idx = i;
  t->info = info;
  t->valid = true;

  if(bits(v, t->bit_idx, 1) == 0) {
    t->left = t;
    t->right = x;
  } else {
    t->right = t;
    t->left = x;
  }

  if(bits(v, p->bit_idx, 1) == 0)
    p->left = t;
  else
    p->right = t;
}


// Returns info based on key
bool
Radix::lookup(KEYTYPE v, INFOTYPE &info)
{
  RadixNode *t;

  t = node_lookup(v);
  if(t->valid) {
    info = t->info;
    return(true);
  } else
    return(false);
}


void
Radix::del(KEYTYPE v)
{
  RadixNode *t;

  t = node_lookup(v);

  // Only delete if this is an exact match
  if(t->key == v)
    t->valid = false;
}



// Returns node based on key
Radix::RadixNode *
Radix::node_lookup(KEYTYPE v)
{
  RadixNode *p, *x = root;

  do {
    p = x;
    if(bits(v, x->bit_idx, 1) == 0)
      x = x->left;
    else
      x = x->right;
  } while(x->bit_idx < p->bit_idx);

  return x;
}



// Returns j bytes which appear k from rhe right. Rightmost bit has index 0.
inline KEYTYPE
Radix::bits(KEYTYPE x, KEYTYPE k, unsigned char j)
{
  return (x >> k) & (0xffffffff >> (KEYSIZE-j));
}

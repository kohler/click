/*
 * radix.{cc,hh} -- a radix tree.
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
  root = new struct node;
  root->rt_idx = root->key = 0;
  root->bit_idx = KEYSIZE-1;
  root->left = root->right = root;
}

Radix::~Radix()
{
}

void
Radix::insert(KEYTYPE v, int rt_idx)
{
  struct node *t, *p;
  int i;

  t = node_lookup(v);

  // In there already?
  if(v == t->key)
    return;

  // t is now the node whose key must be distinguished from v

  // Find bit for which v and t-> key differ
  i = KEYSIZE-1;
  while(bits(v, i, 1) == bits(t->key, i, 1))
    i--;

  // i now equals the index of the bit where v and t->key differ.

  // Travel down the tree to the point where bit_idx <= i
  struct node *x = root;
  do {
    p = x;
    if(bits(v, x->bit_idx, 1) == 0)
      x = x->left;
    else
      x = x->right;

    if((x->bit_idx <= i) || (p->bit_idx <= x->bit_idx))
      break;
  } while(1);

  // Create new node
  t = new struct node;
  t->key = v;
  t->bit_idx = i;
  t->rt_idx = rt_idx;

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


struct Radix::node *
Radix::node_lookup(KEYTYPE v)
{
  struct node *p, *x = root;

  do {
    p = x;
    if(bits(v, x->bit_idx, 1) == 0)
      x = x->left;
    else
      x = x->right;
  } while(x->bit_idx < p->bit_idx);

  return x;
}

int
Radix::lookup(KEYTYPE v)
{
  return(node_lookup(v)->rt_idx);
}

inline KEYTYPE
Radix::bits(KEYTYPE x, KEYTYPE k, unsigned char j)
{
  return (x >> k) & (0xffffffff >> (KEYSIZE-j));
}

/*

int main(int argc, char *argv[])
{
    Radix a;

    a.insert(0x04020101, 0xffffff00, 28);
    a.insert(0x01010101, 0xffff0000, 23);
    a.insert(0x02010101, 0xffff0000, 24);
    a.insert(0x03010101, 0xffff0000, 25);
    a.insert(0x04010101, 0xffff0000, 26);
    a.insert(0x04020101, 0xffff0000, 27);
    printf("%d\n", a.lookup(0x04020101 & 0xffffff00));
}

*/

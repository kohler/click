#ifndef _RADIX_H
#define _RADIX_H


#include "glue.hh"

#define         KEYTYPE              unsigned
#define         KEYSIZE                    32


class Radix {
public:
  Radix();
  ~Radix();

  void insert(KEYTYPE v, int rt_idx);
  int lookup(KEYTYPE v);

private:
  struct node {
    KEYTYPE key;
    int rt_idx;
    int bit_idx;
    struct node *left, *right;
  };

  struct node *root;

  // Make static
  static KEYTYPE bits(KEYTYPE x, KEYTYPE y, unsigned char idx);
  struct node *node_lookup(KEYTYPE v);
};


#endif

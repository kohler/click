#ifndef _RADIX_H
#define _RADIX_H


#include "glue.hh"

#define KEYTYPE   unsigned
#define KEYSIZE   32

#define INFOTYPE  int


class Radix {
public:
  Radix();
  ~Radix();

  void insert(KEYTYPE v, INFOTYPE info);
  void del(KEYTYPE v);
  bool lookup(KEYTYPE v, INFOTYPE &info);

private:
  struct node {
    KEYTYPE key;
    int bit_idx;
    struct node *left, *right;

    bool valid;
    INFOTYPE info;
  };

  struct node *root;

  static KEYTYPE bits(KEYTYPE x, KEYTYPE y, unsigned char idx);
  struct node *node_lookup(KEYTYPE v);
};


#endif

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
  struct RadixNode {
    KEYTYPE key;
    int bit_idx;
    RadixNode *left;
    RadixNode *right;

    bool valid;
    INFOTYPE info;

    RadixNode() : left(0), right(0), valid(false) { }
    void kill();
    
  };

  struct RadixNode *root;

  static KEYTYPE bits(KEYTYPE x, KEYTYPE y, unsigned char idx);
  RadixNode *node_lookup(KEYTYPE v);
};


#endif

// -*- related-file-name: "../../lib/bighashmap_arena.cc" -*-
#ifndef CLICK_BIGHASHMAP_ARENA_HH
#define CLICK_BIGHASHMAP_ARENA_HH

struct BigHashMap_Arena {
  
  enum { SIZE = 128 };
  union {
    int first;
    double padding;		// pad to 8-byte boundary
  } _u;
  unsigned char _x[1];

  static BigHashMap_Arena *new_arena(unsigned esize);
  static void delete_arena(BigHashMap_Arena *);
  void clear()				{ _u.first = 0; }
  void *alloc(unsigned esize);
  void *elt(unsigned esize, int i)	{ return (_x + esize*i); }
  
};

inline void *
BigHashMap_Arena::alloc(unsigned esize)
{
  if (_u.first < SIZE)
    return elt(esize, _u.first++);
  else
    return 0;
}

#endif

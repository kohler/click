#ifndef CLICK_BIGHASHMAP_ARENA_HH
#define CLICK_BIGHASHMAP_ARENA_HH

struct BigHashMap_Arena {
  
  enum { SIZE = 128 };
  int _first;
  int _padding;		// pad to 8-byte boundary
  unsigned char _x[0];

  static BigHashMap_Arena *new_arena(unsigned esize);
  static void delete_arena(BigHashMap_Arena *);
  void clear()				{ _first = 0; }
  void *alloc(unsigned esize);
  void *elt(unsigned esize, int i)	{ return (_x + esize*i); }
  
};

inline void *
BigHashMap_Arena::alloc(unsigned esize)
{
  if (_first < SIZE)
    return elt(esize, _first++);
  else
    return 0;
}

#endif

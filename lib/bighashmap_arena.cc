/*
 * bighashmap_arena.{cc,hh} -- a hash table template that supports removal
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/bighashmap_arena.hh>

#ifdef HAVE_NEW_H
# include <new.h>
#elif !HAVE_PLACEMENT_NEW
inline void *operator new(size_t, void *v) { return v; }
# define HAVE_PLACEMENT_NEW 1
#endif

BigHashMap_Arena *
BigHashMap_Arena::new_arena(unsigned esize)
{
  BigHashMap_Arena *arena =
    reinterpret_cast<BigHashMap_Arena *>(new unsigned char[sizeof(BigHashMap_Arena) + esize*SIZE]);
  if (arena)
    arena->_first = 0;
  return arena;
}

void
BigHashMap_Arena::delete_arena(BigHashMap_Arena *arena)
{
  delete[] reinterpret_cast<unsigned char *>(arena);
}

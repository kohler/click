/*
 * bighashmap_arena.{cc,hh} -- a hash table template that supports removal
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/bighashmap_arena.hh>

BigHashMap_Arena *
BigHashMap_Arena::new_arena(unsigned esize)
{
  BigHashMap_Arena *arena =
    reinterpret_cast<BigHashMap_Arena *>(new unsigned char[offsetof(BigHashMap_Arena, _x) + esize*SIZE]);
  if (arena)
    arena->_u.first = 0;
  return arena;
}

void
BigHashMap_Arena::delete_arena(BigHashMap_Arena *arena)
{
  delete[] reinterpret_cast<unsigned char *>(arena);
}

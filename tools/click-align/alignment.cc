/*
 * alignment.{cc,hh} -- represents alignment constraints
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include "alignment.hh"

bool
Alignment::operator<=(const Alignment &o) const
{
  if (_chunk < 0 || o._chunk < 0)
    return false;
  else if (o._chunk <= 1)
    return true;
  else if (_chunk <= 1)
    return false;
  else if (_chunk % o._chunk != 0)
    return false;
  else
    return (_offset % o._chunk == o._offset);
}

Alignment &
Alignment::operator+=(int delta)
{
  if (_chunk > 1) {
    if (delta < 0)
      delta += ((-delta)/_chunk + 1) * _chunk;
    _offset = (_offset + delta) % _chunk;
  }
  return *this;
}

Alignment &
Alignment::operator|=(const Alignment &o)
{
  if (_chunk == 0 || o._chunk < 0)
    return (*this = o);
  else if (_chunk <= 1 || o._chunk == 0)
    return *this;
  
  // new_chunk = gcd(_chunk, o._chunk)
  int new_chunk = _chunk, b = o._chunk;
  while (b) {			// Euclid's algorithm
    int r = new_chunk % b;
    new_chunk = b;
    b = r;
  }

  // calculate new offsets
  int new_off1 = _offset % new_chunk;
  int new_off2 = o._offset % new_chunk;
  if (new_off1 == new_off2) {
    _chunk = new_chunk;
    _offset = new_off1;
    return *this;
  }

  // check for lowering chunk
  int diff = new_off2 - new_off1;
  if (diff < 0) diff += new_chunk;
  if (diff > new_chunk / 2) diff = new_chunk - diff;
  if (new_chunk % diff == 0) {
    _chunk = diff;
    _offset = new_off1 % diff;
  } else {
    _chunk = 1;
    _offset = 0;
  }
  
  return *this;
}

Alignment &
Alignment::operator&=(const Alignment &o)
{
  // XXX doesn't work for arbitrary alignments; should use some set method
  // and least-common-multiple
  if (o <= *this)
    return (*this = o);
  else if (*this <= o)
    return *this;
  else
    return (*this = Alignment(-1, 0, 0));
}

String
Alignment::unparse() const
{
  if (bad())
    return "BAD";
  else if (empty())
    return "EMPTY";
  else
    return String(_chunk) + "/" + String(_offset);
}

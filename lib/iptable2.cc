/*
 * iptable2.{cc,hh} -- a fast IP routing table. Uses a radix tree for fast
 * routing table lookup.
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
#include "iptable2.hh"
#include "integers.hh"


IPTable2::IPTable2()
  : entries(0), dirty(true)
{
  radix = new Radix;
}

IPTable2::~IPTable2()
{
}


// Adds an entry to the simple routing table if not in there already.
// Allows only one gateway for equals dst/mask combination.
void
IPTable2::add(unsigned dst, unsigned mask, unsigned gw)
{
  for(int i = 0; i < _v.size(); i++)
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask))
      return;

  struct Entry e;
  e._dst = dst;
  e._mask = mask;
  e._gw = gw;
  e._valid = 1;
  _v.push_back(e);
  entries++;

  radix->insert((dst & mask), _v.size()-1);
}


// Deletes an entry from the stupid routing table. Radix is now dirty.
void
IPTable2::del(unsigned dst, unsigned mask)
{
  for(int i = 0; i < _v.size(); i++){
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask)) {
      _v[i]._valid = 0;
      entries--;
      dirty = true;
      return;
    }
  }
}

// Returns the i-th record of the stupid routing table.
bool
IPTable2::get(int i, unsigned &dst, unsigned &mask, unsigned &gw)
{
  assert(i >= 0 && i < _v.size());

  if(i < 0 || i >= _v.size() || _v[i]._valid == 0) {
    dst = mask = gw = 0;
    return(false);
  }

  dst = _v[i]._dst;
  mask = _v[i]._mask;
  gw = _v[i]._gw;
  return(true);
}


// Use the fast routing table to perform the lookup.
bool
IPTable2::lookup(unsigned dst, unsigned &gw, int &index)
{
  if(!entries)
    return false;

  // Use timer.
  if(dirty)
    build();

  index = radix->lookup(dst);

  // Consider this a match if dst is part of range described by routing table.
  if((dst & _v[index]._mask) == (_v[index]._dst & _v[index]._mask)) {
    gw = _v[index]._gw;
    return true;
  }

  gw = index = 0;
  return false;
}


void
IPTable2::build()
{
  radix = new Radix;
  Vector<Entry> newv;

  for(int i = 0; i < _v.size(); i++)
    if(_v[i]._valid)
      newv.push_back(_v[i]);

  _v.clear();
  for(int i = 0; i < newv.size(); i++) {
    _v.push_back(newv[i]);
    radix->insert((_v[i]._dst & _v[i]._mask), i);
  }

  entries = _v.size();
  dirty = false;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IPTable2::Entry>;

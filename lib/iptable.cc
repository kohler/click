/*
 * iptable.{cc,hh} -- a stupid IP routing table, best for small routing tables
 * Robert Morris
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

#include "iptable.hh"

IPTable::IPTable()
{
}

IPTable::~IPTable()
{
}

bool
IPTable::lookup(unsigned dst, unsigned &gw, int &index)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && (dst & _v[i]._mask) == _v[i]._dst){
      if(besti == -1 || ~_v[i]._mask < ~_v[besti]._mask){
        besti = i;
      }
    }
  }

  if(besti == -1){
    return(false);
  } else {
    gw = _v[besti]._gw;
    index = _v[besti]._index;
    return(true);
  }
}

void
IPTable::add(unsigned dst, unsigned mask, unsigned gw, int index)
{
  struct Entry e;

  e._dst = dst;
  e._mask = mask;
  e._gw = gw;
  e._index = index;
  e._valid = 1;

  int i;
  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid == 0){
      _v[i] = e;
      return;
    }
  }
  _v.push_back(e);
}

void
IPTable::del(unsigned dst, unsigned mask)
{
  int i;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && _v[i]._dst == dst && _v[i]._mask == mask){
      _v[i]._valid = 0;
    }
  }
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IPTable::Entry>;

/*
 * iptable2.{cc,hh} -- a fast IP routing table. Implementation based on "Small
 * Forwarding Tables for Fast Routing Lookups" by Mikael Degermark, Andrej
 * Brodnik, Svante Carlsson and Stephen Pink.
 * Thomer M. Gil
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

IPTable2::IPTable2()
  : entries(0)
{
  build_maptable();
}

IPTable2::~IPTable2()
{
}


void
IPTable2::build_maptable()
{
  click_chatter("%d", sizeof(_maptable));
}


bool
IPTable2::exists(unsigned dst, unsigned mask, unsigned gw)
{
  int dummy;
  return search(dst, mask, gw, dummy);
}


bool
IPTable2::search(unsigned dst, unsigned mask, unsigned gw, int &index)
{
  for(int i = 0; i < _v.size(); i++){
    if(_v[i]._valid && _v[i]._dst == dst && _v[i]._mask == mask &&
       _v[i]._gw == gw){
      index = i;
      return(true);
    }
  }

  return(false);
}


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

void
IPTable2::add(unsigned dst, unsigned mask, unsigned gw)
{
  struct Entry e;

  e._dst = dst;
  e._mask = mask;
  e._gw = gw;
  e._valid = 1;

  int i;
  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid == 0){
      _v[i] = e;
      entries++;
      return;
    }
  }
  _v.push_back(e);
  entries++;
}

int
IPTable2::size() const
{
  return entries;
}


void
IPTable2::del(unsigned dst, unsigned mask)
{
  int i;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && _v[i]._dst == dst && _v[i]._mask == mask){
      _v[i]._valid = 0;
      entries--;
      return;   // add ensures there can be only be 1 match.
    }
  }
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IPTable2::Entry>;

/*
 * iptable.{cc,hh} -- a stupid IP routing table, best for small routing tables
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/iptable.hh>

IPTable::IPTable()
{
}

IPTable::~IPTable()
{
}

bool
IPTable::lookup(IPAddress dst, IPAddress &gw, int &index) const
{
  int best = -1;

  // longest prefix match
  for (int i = 0; i < _v.size(); i++)
    if (dst.matches_prefix(_v[i].dst, _v[i].mask)) {
      if (best < 0 || _v[i].mask.mask_more_specific(_v[best].mask))
	best = i;
    }

  if (best < 0)
    return false;
  else {
    gw = _v[best].gw;
    index = _v[best].index;
    return true;
  }
}

void
IPTable::add(IPAddress dst, IPAddress mask, IPAddress gw, int index)
{
  dst &= mask;

  struct Entry e;
  e.dst = dst;
  e.mask = mask;
  e.gw = gw;
  e.index = index;
  
  for (int i = 0; i < _v.size(); i++)
    if (!_v[i].valid()) {
      _v[i] = e;
      return;
    }
  _v.push_back(e);
}

void
IPTable::del(IPAddress dst, IPAddress mask)
{
  for (int i = 0; i < _v.size(); i++)
    if (_v[i].dst == dst && _v[i].mask == mask) {
      _v[i].dst = IPAddress(1);
      _v[i].mask = IPAddress(0);
    }
}

// generate Vector template instance
#include <click/vector.cc>
template class Vector<IPTable::Entry>;

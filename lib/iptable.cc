// -*- c-basic-offset: 2; related-file-name: "../include/click/iptable.hh" -*-
/*
 * iptable.{cc,hh} -- a stupid IP routing table, best for small routing tables
 * Robert Morris
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
#include <click/iptable.hh>
CLICK_DECLS

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
      if (best < 0 || _v[i].mask.mask_as_specific(_v[best].mask))
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

CLICK_ENDDECLS

/*
 * ipaddressset.{cc,hh} -- a set of IP addresses
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include <click/config.h>

#include <click/ipaddressset.hh>
#include <click/glue.hh>
#include <click/confparse.hh>

void
IPAddressSet::insert(IPAddress ip)
{
  unsigned ipu = ip;
  for (int i = 0; i < _s.size(); i++)
    if (_s[i] == ipu)
      return;
  _s.push_back(ipu);
}

bool
IPAddressSet::find(IPAddress ip) const
{
  unsigned ipu = ip;
  for (int i = 0; i < _s.size(); i++)
    if (_s[i] == ipu)
      return true;
  return false;
}

unsigned *
IPAddressSet::list_copy()
{
  if (!_s.size())
    return 0;
  unsigned *x = new unsigned[_s.size()];
  if (x)
    memcpy(x, &_s[0], sizeof(unsigned) * _s.size());
  return x;
}

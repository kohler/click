/*
 * ipaddressset.{cc,hh} -- a set of IP addresses
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipaddressset.hh"
#include "glue.hh"
#include "confparse.hh"

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

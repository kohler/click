// -*- c-basic-offset: 2; related-file-name: "../include/click/ipaddressset.hh" -*-
/*
 * ipaddressset.{cc,hh} -- a set of IP addresses
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <click/ipaddressset.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
CLICK_DECLS

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

CLICK_ENDDECLS

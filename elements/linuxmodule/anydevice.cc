/*
 * anydevice.{cc,hh} -- support Linux device interaction
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/config.h>
#include <click/package.hh>
#include <click/glue.hh>
#include "anydevice.hh"
#include <click/confparse.hh>

AnyDevice::AnyDevice()
  : _dev(0), _next(0)
{
  MOD_INC_USE_COUNT;
}

AnyDevice::~AnyDevice()
{
  MOD_DEC_USE_COUNT;
}

void
AnyDeviceMap::initialize()
{
  for (int i = 0; i < MAX_DEVICES; i++)
    _map[i] = 0;
}

int
AnyDeviceMap::insert(AnyDevice *d)
{
  int ifi = d->ifindex();
  if (ifi < 0 || ifi >= MAX_DEVICES)
    return -1;
  // put new devices first on the list
  d->set_next(_map[ifi]);
  _map[ifi] = d;
  return 0;
}

void
AnyDeviceMap::remove(AnyDevice *d)
{
  int ifi = d->ifindex();
  if (ifi < 0 || ifi >= MAX_DEVICES)
    return;

  AnyDevice *prev = 0;
  AnyDevice *trav = _map[ifi];
  while (trav && trav != d) {
    prev = trav;
    trav = trav->next();
  }
  if (trav) {
    if (prev)
      prev->set_next(trav->next());
    else
      _map[ifi] = trav->next();
  }
}


struct device *
find_device_by_ether_address(const String &name, Element *context)
{
  unsigned char en[6];
  if (!cp_ethernet_address(name, en, context))
    return 0;
  for (struct device *dev = dev_base; dev; dev = dev->next)
    if (dev->addr_len == 6 && memcmp(en, dev->dev_addr, 6) == 0)
      return dev;
  return 0;
}

ELEMENT_REQUIRES(linuxmodule)
ELEMENT_PROVIDES(AnyDevice)

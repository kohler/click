/*
 * anydevice.{cc,hh} -- support Linux device interaction
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
#include "glue.hh"
#include "anydevice.hh"

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

ELEMENT_PROVIDES(AnyDevice)

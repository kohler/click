/*
 * anydevice.{cc,hh} -- support Linux device interaction
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/glue.hh>
#include "anydevice.hh"
#include <click/confparse.hh>

AnyDevice::AnyDevice()
  : _dev(0), _task(this), _idles(0), _next(0)
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
    _unknown_map = 0;
    for (int i = 0; i < MAX_DEVICES; i++)
	_map[i] = 0;
}

int
AnyDeviceMap::insert(AnyDevice *d)
{
    int ifi = d->ifindex();
    AnyDevice **head;
    if (ifi < 0)
	head = &_unknown_map;
    else if (ifi < MAX_DEVICES)
	head = &_map[ifi];
    else
	return -1;
    // put new devices first on the list
    d->set_next(*head);
    *head = d;
    return 0;
}

void
AnyDeviceMap::remove(AnyDevice *d)
{
    int ifi = d->ifindex();
    AnyDevice **head = (ifi >= 0 && ifi < MAX_DEVICES ? &_map[ifi] : &_unknown_map);

    AnyDevice *prev = 0;
    AnyDevice *trav = *head;
    while (trav && trav != d) {
	prev = trav;
	trav = trav->next();
    }
    if (trav) {
	if (prev)
	    prev->set_next(trav->next());
	else
	    *head = trav->next();
    }
}

AnyDevice *
AnyDeviceMap::lookup_unknown(net_device *dev)
{
    // look first by device names
    String dev_name = dev->name;
    for (AnyDevice *d = _unknown_map; d; d = d->next())
	if (d->devname() == dev_name)
	    return d;

    // then by Ethernet addresses
    if (dev->type == ARPHRD_ETHER) {
	unsigned char en[6];
	for (AnyDevice *d = _unknown_map; d; d = d->next())
	    if (cp_ethernet_address(d->devname(), en, d))
		if (memcmp(en, dev->dev_addr, 6) == 0)
		    return d;
    }

    return 0;
}


net_device *
find_device_by_ether_address(const String &name, Element *context)
{
  unsigned char en[6];
  if (!cp_ethernet_address(name, en, context))
    return 0;
  for (net_device *dev = dev_base; dev; dev = dev->next)
    if (dev->type == ARPHRD_ETHER && memcmp(en, dev->dev_addr, 6) == 0)
      return dev;
  return 0;
}

ELEMENT_REQUIRES(linuxmodule)
ELEMENT_PROVIDES(AnyDevice)

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
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/smp_lock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

AnyDevice::AnyDevice()
  : _dev(0), _task(this), _idles(0), _next(0)
{
  MOD_INC_USE_COUNT;
}

AnyDevice::~AnyDevice()
{
  MOD_DEC_USE_COUNT;
}

int
AnyDevice::find_device(bool allow_nonexistent, ErrorHandler *errh)
{
    _dev = dev_get_by_name(_devname.cc());
    if (!_dev)
	_dev = find_device_by_ether_address(_devname, this);
    if (!_dev) {
	if (!allow_nonexistent)
	    return errh->error("unknown device `%s'", _devname.cc());
	else
	    errh->warning("unknown device `%s'", _devname.cc());
    }
    if (_dev && !(_dev->flags & IFF_UP)) {
	errh->warning("device `%s' is down", _devname.cc());
	_dev = 0;
    }
    return 0;
}

void
AnyDeviceMap::initialize()
{
    _unknown_map = 0;
    for (int i = 0; i < MAP_SIZE; i++)
	_map[i] = 0;
}

void
AnyDeviceMap::insert(AnyDevice *d)
{
    // lock whole kernel when manipulating device map
    lock_kernel();
    
    int ifi = d->ifindex();
    AnyDevice **head;
    if (ifi < 0)
	head = &_unknown_map;
    else
	head = &_map[ifi % MAP_SIZE];

    // put new devices first on the list
    d->set_next(*head);
    *head = d;

    unlock_kernel();
}

void
AnyDeviceMap::remove(AnyDevice *d)
{
    lock_kernel();
    
    int ifi = d->ifindex();
    AnyDevice **head = (ifi >= 0 ? &_map[ifi % MAP_SIZE] : &_unknown_map);
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

    unlock_kernel();
}

AnyDevice *
AnyDeviceMap::lookup_unknown(net_device *dev)
{
    // make sure device is valid
    if (dev == NULL)
	return NULL;

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

/*
 * anydevice.{cc,hh} -- support BSD device interaction
 * Eddie Kohler, Nickolai Zeldovich
 *
 * Copyright (c) 2000, 2001 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/etheraddress.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/lock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
CLICK_DECLS

struct ifnet _dev_click = {};

AnyDevice::AnyDevice()
  : _dev(0), _task(this), _next(0), _idles(0)
{
}

AnyDevice::~AnyDevice()
{
}

static void
lock_kernel()
{
    // XXX not yet in BSD
}

static void
unlock_kernel()
{
    // XXX not yet in BSD
}

int
AnyDevice::find_device(bool allow_nonexistent, AnyDeviceMap *adm,
                       ErrorHandler *errh)
{
    _dev = ifunit((char *) _devname.c_str());
    if (!_dev)
	_dev = find_device_by_ether_address(_devname, this);
    if (!_dev) {
	if (!allow_nonexistent)
	    return errh->error("unknown device `%s'", _devname.c_str());
	else
	    errh->warning("unknown device `%s'", _devname.c_str());
    }
    if (_dev && !(_dev->if_flags & IFF_UP)) {
	errh->warning("device `%s' is down", _devname.c_str());
	_dev = 0;
    }

    return 0;
}

void
AnyDevice::clear_device(AnyDeviceMap *adm)
{
#if 0 /* MARKO XXX */
    if (_dev && _promisc)
        dev_set_promiscuity(_dev, -1);
#endif

    if (adm)
        adm->remove(this);
    _dev = 0;
}

AnyTaskDevice::AnyTaskDevice()
    : _task(this), _idles(0)
{
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
AnyDeviceMap::lookup_unknown(struct ifnet *dev)
{
    // make sure device is valid
    if (dev == NULL)
	return NULL;

    // look first by device names
    String dev_name = dev->if_xname;
    for (AnyDevice *d = _unknown_map; d; d = d->next())
	if (d->devname() == dev_name)
	    return d;

#if 0 /* XXX this is slightly more complicated in BSD */
    // then by Ethernet addresses
    if (dev->if_type == IFT_ETHER) {
	unsigned char en[6];
	for (AnyDevice *d = _unknown_map; d; d = d->next())
	    if (EtherAddressArg().parse(d->devname(), en, d))
		if (memcmp(en, dev->dev_addr, 6) == 0)
		    return d;
    }
#endif

    return 0;
}


struct ifnet *
find_device_by_ether_address(const String &name, Element *context)
{
#if 0 /* XXX slightly more difficult in BSD */
  unsigned char en[6];
  if (!EtherAddressArg().parse(name, en, context))
    return 0;
  for (struct ifnet *dev = dev_base; dev; dev = dev->next)
    if (dev->type == ARPHRD_ETHER && memcmp(en, dev->dev_addr, 6) == 0)
      return dev;
#endif
  return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(bsdmodule)
ELEMENT_PROVIDES(AnyDevice)

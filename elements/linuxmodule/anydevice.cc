// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * anydevice.{cc,hh} -- support Linux device interaction
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
    : _dev(0), _task(this), _idles(0), _flags(0), _next(0)
{
    MOD_INC_USE_COUNT;
}

AnyDevice::~AnyDevice()
{
    if (flag(F_IN_MAP) || _dev)
	click_chatter("%s: bad device destructor!", id().cc());
    MOD_DEC_USE_COUNT;
}

int
AnyDevice::find_device(bool allow_nonexistent, AnyDeviceMap *adm,
		       ErrorHandler *errh)
{
    _dev = dev_get_by_name(_devname.cc());
    if (!_dev)
	_dev = dev_get_by_ether_address(_devname, this);
    if (!_dev) {
	if (!allow_nonexistent)
	    return errh->error("unknown device `%s'", _devname.cc());
	else
	    errh->warning("unknown device `%s'", _devname.cc());
    }
    if (_dev && !(_dev->flags & IFF_UP)) {
	errh->warning("device `%s' is down", _devname.cc());
	dev_put(_dev);
	_dev = 0;
    }

    if (_dev && flag(F_PROMISC))
	dev_set_promiscuity(_dev, 1);
    
    set_flag(F_IN_MAP);
    adm->insert(this);

    return 0;
}

void
AnyDevice::set_device(net_device *dev, AnyDeviceMap *adm)
{
    if (_dev)
	click_chatter("%s: device `%s' went down", declaration().cc(), _devname.cc());
    if (dev)
	click_chatter("%s: device `%s' came up", declaration().cc(), _devname.cc());

    if (_dev && flag(F_PROMISC))
	dev_set_promiscuity(_dev, -1);
    
    if (adm && flag(F_IN_MAP))
	adm->remove(this);
    if (_dev)
	dev_put(_dev);
    _dev = dev;
    if (_dev)
	dev_hold(_dev);
    if (adm) {
	set_flag(F_IN_MAP);
	adm->insert(this);
    }

    if (_dev && flag(F_PROMISC))
	dev_set_promiscuity(_dev, 1);
}

void
AnyDevice::clear_device(AnyDeviceMap *adm)
{
    if (_dev && flag(F_PROMISC))
	dev_set_promiscuity(_dev, -1);
    
    if (adm) {
	clear_flag(F_IN_MAP);
	adm->remove(this);
    }
    if (_dev)
	dev_put(_dev);
    _dev = 0;
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
    
    // put new devices last on list
    int ifi = d->ifindex();
    AnyDevice **pprev = (ifi >= 0 ? &_map[ifi % MAP_SIZE] : &_unknown_map);
    AnyDevice *trav = *pprev;
    while (trav) {
	pprev = &trav->_next;
	trav = *pprev;
    }
    d->_next = 0;
    *pprev = d;

    unlock_kernel();
}

void
AnyDeviceMap::move_to_front(AnyDevice *d)
{
    // lock whole kernel when manipulating device map
    lock_kernel();
    
    // put new devices last on list
    int ifi = d->ifindex();
    AnyDevice **pprev = (ifi >= 0 ? &_map[ifi % MAP_SIZE] : &_unknown_map);
    AnyDevice **head = pprev;
    AnyDevice *trav = *pprev;
    while (trav && trav != d) {
	pprev = &trav->_next;
	trav = *pprev;
    }
    if (trav)
	*pprev = d->_next;
    d->_next = *head;
    *head = d;

    unlock_kernel();
}

void
AnyDeviceMap::remove(AnyDevice *d)
{
    lock_kernel();
    
    int ifi = d->ifindex();
    AnyDevice **pprev = (ifi >= 0 ? &_map[ifi % MAP_SIZE] : &_unknown_map);
    AnyDevice *trav = *pprev;
    while (trav && trav != d) {
	pprev = &trav->_next;
	trav = *pprev;
    }
    if (trav)
	*pprev = d->_next;

    unlock_kernel();
}

AnyDevice *
AnyDeviceMap::lookup_unknown(net_device *dev, AnyDevice *last)
{
    // make sure device is valid
    if (!dev)
	return 0;

    // look by device name and Ethernet address
    String dev_name = dev->name;
    unsigned char en[6];
    
    for (AnyDevice *d = (last ? last : _unknown_map); d; d = d->_next)
	if (d->devname() == dev_name)
	    return d;
	else if (dev->type == ARPHRD_ETHER
		 && cp_ethernet_address(d->devname(), en, d)
		 && memcmp(en, dev->dev_addr, 6) == 0)
	    return d;

    return 0;
}


net_device *
dev_get_by_ether_address(const String &name, Element *context)
{
    unsigned char en[6];
    if (!cp_ethernet_address(name, en, context))
	return 0;
    for (net_device *dev = dev_base; dev; dev = dev->next)
	if (dev->type == ARPHRD_ETHER && memcmp(en, dev->dev_addr, 6) == 0) {
	    dev_hold(dev);	// dev_get_by_name does dev_hold; so
				// should we
	    return dev;
	}
    return 0;
}

ELEMENT_REQUIRES(linuxmodule)
ELEMENT_PROVIDES(AnyDevice)

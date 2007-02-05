// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * anydevice.{cc,hh} -- support Linux device interaction
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004-2005 Regents of the University of California
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
#include <clicknet/ether.h>
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/if_arp.h>
#endif
#include <linux/smp_lock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

AnyDevice::AnyDevice()
    : _dev(0), _promisc(false), _in_map(false), _next(0)
{
}

AnyDevice::~AnyDevice()
{
    if (_in_map || _dev)
	click_chatter("%s: bad device destructor!", name().c_str());
}

int
AnyDevice::find_device(bool allow_nonexistent, AnyDeviceMap *adm,
		       ErrorHandler *errh)
{
    _dev = dev_get_by_name(_devname.c_str());
    if (!_dev)
	_dev = dev_get_by_ether_address(_devname, this);
    if (!_dev) {
	if (!allow_nonexistent)
	    return errh->error("unknown device '%s'", _devname.c_str());
	else
	    errh->warning("unknown device '%s'", _devname.c_str());
    }
    if (_dev && !(_dev->flags & IFF_UP)) {
	errh->warning("device '%s' is down", _devname.c_str());
	dev_put(_dev);
	_dev = 0;
    }

    if (_dev && _promisc)
	dev_set_promiscuity(_dev, 1);

    if (adm)
	adm->insert(this, false);

    return 0;
}

void
AnyDevice::set_device(net_device *dev, AnyDeviceMap *adm, bool locked)
{
    if (_dev == dev)		// changing to the same device is a noop
	return;
    
    if (_dev)
	click_chatter("%s: device '%s' went down", declaration().c_str(), _devname.c_str());
    if (dev)
	click_chatter("%s: device '%s' came up", declaration().c_str(), _devname.c_str());

    if (_dev && _promisc)
	dev_set_promiscuity(_dev, -1);
    
    if (adm && _in_map)
	adm->remove(this, locked);
    if (_dev)
	dev_put(_dev);
    _dev = dev;
    if (_dev)
	dev_hold(_dev);
    if (adm)
	adm->insert(this, locked);

    if (_dev && _promisc)
	dev_set_promiscuity(_dev, 1);
}

void
AnyDevice::clear_device(AnyDeviceMap *adm)
{
    if (_dev && _promisc)
	dev_set_promiscuity(_dev, -1);
    if (adm && _in_map)
	adm->remove(this, false);
    if (_dev)
	dev_put(_dev);
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
    rwlock_init(&_lock);
}

void
AnyDeviceMap::insert(AnyDevice *d, bool locked)
{
    // lock when manipulating device map
    if (!locked)
	lock(true);
    
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

    d->_in_map = true;
    if (!locked)
	unlock(true);
}

void
AnyDeviceMap::remove(AnyDevice *d, bool locked)
{
    if (!locked)
	lock(true);
    int ifi = d->ifindex();
    AnyDevice **pprev = (ifi >= 0 ? &_map[ifi % MAP_SIZE] : &_unknown_map);
    AnyDevice *trav = *pprev;
    while (trav && trav != d) {
	pprev = &trav->_next;
	trav = *pprev;
    }
    if (trav)
	*pprev = d->_next;
    d->_in_map = false;
    if (!locked)
	unlock(true);
}

AnyDevice *
AnyDeviceMap::lookup_unknown(net_device *dev, AnyDevice *last) const
    // must be called between AnyDeviceMap::lock() ... unlock()
{
    // make sure device is valid
    if (!dev)
	return 0;

    // look by device name and Ethernet address
    String dev_name = dev->name;
    unsigned char en[6];
    
    for (AnyDevice *d = (last ? last->_next : _unknown_map); d; d = d->_next)
	if (d->devname() == dev_name)
	    return d;
	else if ((dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)
		 && cp_ethernet_address(d->devname(), en, d)
		 && memcmp(en, dev->dev_addr, 6) == 0)
	    return d;

    return 0;
}

void
AnyDeviceMap::lookup_all(net_device *dev, bool known, Vector<AnyDevice *> &v) const
    // must be called between AnyDeviceMap::lock() ... unlock()
{
    if (known)
	for (AnyDevice *d = 0; d = lookup(dev, d); v.push_back(d))
	    /* nada */;
    else
	for (AnyDevice *d = 0; d = lookup_unknown(dev, d); v.push_back(d))
	    /* nada */;
}


net_device *
dev_get_by_ether_address(const String &name, Element *context)
{
    unsigned char en[6];
    if (!cp_ethernet_address(name, en, context))
	return 0;
    read_lock(&dev_base_lock);
    net_device *dev;
    for (dev = dev_base; dev; dev = dev->next)
	if ((dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)
	    && memcmp(en, dev->dev_addr, 6) == 0) {
	    dev_hold(dev);	// dev_get_by_name does dev_hold; so
				// should we
	    break;
	}
    read_unlock(&dev_base_lock);
    return dev;
}

ELEMENT_REQUIRES(linuxmodule)
ELEMENT_PROVIDES(AnyDevice)

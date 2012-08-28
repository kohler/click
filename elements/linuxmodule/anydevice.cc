// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * anydevice.{cc,hh} -- support Linux device interaction
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004-2008 Regents of the University of California
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
#include "fromdevice.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/handlercall.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/if_arp.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# include <linux/rtnetlink.h>
# include <net/net_namespace.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if CLICK_DEVICE_UNRECEIVABLE_SK_BUFF
extern "C" {
DEFINE_PER_CPU(sk_buff *, click_device_unreceivable_sk_buff);
}
#endif

AnyDevice::AnyDevice()
    : _dev(0), _promisc(false), _timestamp(true), _in_map(false),
      _quiet(false), _allow_nonexistent(false), _devname_exists(false),
      _carrier_ok(false), _next(0), _up_call(0), _down_call(0)
{
}

AnyDevice::~AnyDevice()
{
    if (_in_map || _dev)
	click_chatter("%s: bad device destructor!", name().c_str());
    delete _up_call;
    delete _down_call;
}

int
AnyDevice::configure_keywords(Vector<String> &conf, ErrorHandler *errh,
			      bool is_reader)
{
    bool allow_nonexistent = _allow_nonexistent;
    bool quiet = _quiet;
    bool promisc = _promisc;
    bool timestamp = _timestamp;
    HandlerCall up_call, down_call;

    if (Args(this, errh).bind(conf)
	.read("UP_CALL", HandlerCallArg(HandlerCall::writable), up_call)
	.read("DOWN_CALL", HandlerCallArg(HandlerCall::writable), down_call)
	.read("ALLOW_NONEXISTENT", allow_nonexistent)
	.read("QUIET", quiet)
	.consume() < 0)
	return -1;
    if (is_reader && (Args(this, errh).bind(conf)
		      .read("PROMISC", promisc)
		      .read("TIMESTAMP", timestamp)
		      .consume() < 0))
	return -1;

    _allow_nonexistent = allow_nonexistent;
    _quiet = quiet;
    _promisc = promisc;
    _timestamp = timestamp;
    delete _up_call;
    _up_call = up_call ? new HandlerCall(up_call) : 0;
    delete _down_call;
    _down_call = down_call ? new HandlerCall(down_call) : 0;
    return 0;
}

int
AnyDevice::initialize_keywords(ErrorHandler *errh)
{
    if ((_up_call && _up_call->initialize_write(this, errh) < 0)
	|| (_down_call && _down_call->initialize_write(this, errh) < 0))
	return -1;
    if (_up_call && _carrier_ok)
	_up_call->call_write(errh);
    else if (_down_call && !_carrier_ok)
	_down_call->call_write(errh);
    return 0;
}

void
AnyDevice::alter_promiscuity(int delta)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    rtnl_lock();
    dev_set_promiscuity(_dev, delta);
    rtnl_unlock();
#else
    dev_set_promiscuity(_dev, delta);
#endif
}

void
AnyDevice::alter_from_device(int delta)
{
#if HAVE_CLICK_KERNEL
    (void) delta;
#elif CLICK_FROMDEVICE_USE_BRIDGE
    if (!_dev)
	return;
    fake_bridge *fb = reinterpret_cast<fake_bridge *>(_dev->br_port);
    if (fb && fb->magic != fake_bridge::click_magic)
	printk("<1>%s: appears to be owned by the bridge module!", _devname.c_str());
    else if (delta < 0 && fb && atomic_dec_and_test(&fb->refcount)) {
	delete fb;
	rcu_assign_pointer(_dev->br_port, NULL);
    } else if (delta > 0 && !fb) {
	fb = new fake_bridge;
	fb->magic = fake_bridge::click_magic;
	atomic_set(&fb->refcount, 1);
	rcu_assign_pointer(_dev->br_port, reinterpret_cast<struct net_bridge_port *>(fb));
    } else if (delta > 0)
	atomic_inc(&fb->refcount);
#elif HAVE_LINUX_NETDEV_RX_HANDLER_REGISTER
    if (!_dev)
	return;
    rtnl_lock();
    if (_dev->rx_handler && _dev->rx_handler != click_fromdevice_rx_handler)
	printk("<1>%s: rx_handler already set!", _devname.c_str());
    else if (delta < 0 && !_dev->rx_handler_data)
	netdev_rx_handler_unregister(_dev);
    else if (delta > 0 && !_dev->rx_handler)
	netdev_rx_handler_register(_dev, click_fromdevice_rx_handler, 0);
    else
	_dev->rx_handler_data = (void *) ((uintptr_t) _dev->rx_handler_data + delta);
    rtnl_unlock();
#else
    (void) delta;
#endif
}

net_device *
AnyDevice::lookup_device(ErrorHandler *errh)
{
    net_device *dev = get_by_name(_devname.c_str());
    _devname_exists = (bool) dev;
    if (!dev)
	dev = get_by_ether_address(_devname, this);

    if (!dev && !_allow_nonexistent)
	errh->error("unknown device %<%s%>", _devname.c_str());
    else if (!dev && !_quiet)
	errh->warning("unknown device %<%s%>", _devname.c_str());
    else if (dev && !(dev->flags & IFF_UP)) {
	if (!_quiet)
	    errh->warning("device %<%s%> is down", _devname.c_str());
	dev_put(dev);
	dev = 0;
    }
    return dev;
}

void
AnyDevice::set_device(net_device *dev, AnyDeviceMap *adm, int flags)
{
     if (!dev && !_dev && !_in_map && adm) {
	adm->insert(this, flags & anydev_change);
	return;
    }

   if (_dev == dev) {		// no device change == carrier sense only
	bool carrier_ok = (_dev && netif_carrier_ok(_dev));
	if (carrier_ok != _carrier_ok) {
	    _carrier_ok = carrier_ok;
	    if (_down_call && !_carrier_ok && (flags & anydev_change))
		_down_call->call_write(ErrorHandler::default_handler());
	    if (_up_call && _carrier_ok && (flags & anydev_change))
		_up_call->call_write(ErrorHandler::default_handler());
	}
	return;
    }

    // call going-down notifiers
    if (_dev) {
	if (_down_call && _carrier_ok && (flags & anydev_change))
	    _down_call->call_write(ErrorHandler::default_handler());
	if (!_down_call && !_quiet)
	    click_chatter("%s: device %<%s%> went down", declaration().c_str(), _devname.c_str());
    }

    clear_device(adm, flags);

    _dev = dev;
    if (_dev && (flags & anydev_change))
	dev_hold(_dev);
    if (adm)
	adm->insert(this, flags & anydev_change);

    if (_dev && _promisc)
	alter_promiscuity(1);
#if HAVE_NET_ENABLE_TIMESTAMP
    if (_dev && _timestamp)
	net_enable_timestamp();
#endif
    if (_dev && (flags & anydev_from_device))
	alter_from_device(1);
    _carrier_ok = (_dev && netif_carrier_ok(_dev));

    // call going-up notifiers
    if (_dev && (flags & anydev_change)) {
	if (_up_call && _carrier_ok)
	    _up_call->call_write(ErrorHandler::default_handler());
	if (!_up_call && !_quiet)
	    click_chatter("%s: device %<%s%> came up", declaration().c_str(), _devname.c_str());
    }
}

void
AnyDevice::clear_device(AnyDeviceMap *adm, int flags)
{
    if (_dev && _promisc)
	alter_promiscuity(-1);
#if HAVE_NET_ENABLE_TIMESTAMP
    if (_dev && _timestamp)
	net_disable_timestamp();
#endif
    if (_dev && (flags & anydev_from_device))
	alter_from_device(-1);
    if (adm && _in_map)
	adm->remove(this, flags & anydev_change);
    if (_dev)
	dev_put(_dev);
    _dev = 0;
    _carrier_ok = false;
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
    unsigned long lock_flags;
    if (!locked)
	lock(true, lock_flags);

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
	unlock(true, lock_flags);
}

void
AnyDeviceMap::remove(AnyDevice *d, bool locked)
{
    unsigned long lock_flags;
    if (!locked)
	lock(true, lock_flags);
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
	unlock(true, lock_flags);
}

AnyDevice *
AnyDeviceMap::lookup_unknown(net_device *dev, AnyDevice *last) const
    // must be called between AnyDeviceMap::lock() ... unlock()
{
    // make sure device is valid
    if (!dev)
	return 0;

    // look by device name and Ethernet address
    int dev_name_len = strlen(dev->name);
    unsigned char en[6];

    for (AnyDevice *d = (last ? last->_next : _unknown_map); d; d = d->_next)
	if (d->devname().equals(dev->name, dev_name_len)) {
	    d->_devname_exists = true;
	    return d;
	} else if ((dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)
		   && !d->_devname_exists
		   && EtherAddressArg().parse(d->devname(), en, d)
		   && memcmp(en, dev->dev_addr, 6) == 0)
	    return d;

    return 0;
}

int
AnyDeviceMap::lookup_all(net_device *dev, bool known, AnyDevice **dev_store,
			 int ndev) const
    // must be called between AnyDeviceMap::lock() ... unlock()
{
    int i = 0;
    if (known)
	for (AnyDevice *d = 0; i < ndev && (d = lookup(dev, d)); ++i)
	    dev_store[i] = d;
    else
	for (AnyDevice *d = 0; i < ndev && (d = lookup_unknown(dev, d)); ++i)
	    dev_store[i] = d;
    return i;
}


net_device *
AnyDevice::get_by_ether_address(const String &name, Element *context)
{
    unsigned char en[6];
    if (!EtherAddressArg().parse(name, en, context))
	return 0;
    read_lock(&dev_base_lock);
    net_device *dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    for_each_netdev(&init_net, dev)
#else
    for (dev = dev_base; dev; dev = dev->next)
#endif
	if ((dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)
	    && memcmp(en, dev->dev_addr, 6) == 0) {
	    dev_hold(dev);	// dev_get_by_name does dev_hold; so
				// should we
	    break;
	}
    read_unlock(&dev_base_lock);
    return dev;
}

ELEMENT_REQUIRES(linuxmodule FromDevice)
ELEMENT_PROVIDES(AnyDevice)

// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromlinux.{cc,hh} -- receives packets from Linux
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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
#include <click/router.hh>
#include "fromlinux.hh"
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <net/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#define netif_start_queue(dev)	do { dev->start=1; dev->tbusy=0; } while(0)
#define netif_stop_queue(dev)	do { dev->start=0; dev->tbusy=1; } while(0)
#endif

static int fl_open(net_device *);
static int fl_close(net_device *);
static int fl_tx(struct sk_buff *, net_device *);
static net_device_stats *fl_stats(net_device *);
static void fl_wakeup(Timer *, void *);

static int from_linux_count;
static AnyDeviceMap fromlinux_map;

static void
fromlinux_static_initialize()
{
    from_linux_count++;
    if (from_linux_count == 1)
	fromlinux_map.initialize();
}

static void
fromlinux_static_cleanup()
{
    from_linux_count--;
}

FromLinux::FromLinux()
    : _macaddr((const unsigned char *)"\000\001\002\003\004\005"),
      _wakeup_timer(fl_wakeup, this)
{
    MOD_INC_USE_COUNT;
    fromlinux_static_initialize();
    add_output();
    memset(&_stats, 0, sizeof(_stats));
}

FromLinux::~FromLinux()
{
    uninitialize();
    fromlinux_static_cleanup();
    MOD_DEC_USE_COUNT;
}

FromLinux *
FromLinux::clone() const
{
    return new FromLinux;
}

static net_device *
new_fromlinux_device(const char *name)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    read_lock(&dev_base_lock);
#endif
    int errcode;
    net_device *dev = dev_alloc(name, &errcode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    read_unlock(&dev_base_lock);
#endif
    if (!dev)
	return 0;
    
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
    // need to zero out the dev structure
    char *nameptr = dev->name;
    memset(dev, 0, sizeof(*dev));
    dev->name = nameptr;
#endif

    ether_setup(dev);
    dev->open = fl_open;
    dev->stop = fl_close;
    dev->hard_start_xmit = fl_tx;
    dev->get_stats = fl_stats;
    dev->priv = (void *)1;
    return dev;
}

int
FromLinux::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_parse(conf, this, errh,
		    cpString, "interface name", &_devname,
		    cpIPPrefix, "destination IP prefix", &_destaddr, &_destmask,
		    cpKeywords,
		    "ETHER", cpEthernetAddress, "fake device Ethernet address", &_macaddr,
		    cpEnd) < 0)
	return -1;
    if (_devname.length() > IFNAMSIZ - 1)
	return errh->error("device name `%s' too long", _devname.cc());

    // check for duplicate element
    void *&used = router()->force_attachment("FromLinux_" + _devname);
    if (used)
	return errh->error("duplicate FromLinux for device `%s'", _devname.cc());
    used = this;
    
    // check for existing device
    _dev = dev_get_by_name(_devname.cc());
    if (_dev) {
	if (_dev->open != fl_open) {
	    dev_put(_dev);
	    _dev = 0;
	    return errh->error("device `%s' already exists", _devname.cc());
	} else {
	    //lock_kernel();	// XXX too much locking
	    uint32_t count = (uint32_t)(_dev->priv);
	    _dev->priv = (void *)(count + 1);
	    //unlock_kernel();
	    return 0;
	}
    }

    // if not found, create new device
    int res;
    _dev = new_fromlinux_device(_devname);
    if (!_dev)
	return errh->error("out of memory!", res, _devname.cc());
    else if ((res = register_netdev(_dev)) < 0) {
	kfree(_dev);
	_dev = 0;
	return errh->error("error %d registering device `%s'", res, _devname.cc());
    }

    dev_hold(_dev);
    return 0;
}

static void
dev_locks(int up)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    if (up > 0)
	rtnl_lock();
    else
	rtnl_unlock();
#endif
}
    
int
FromLinux::set_device_addresses(ErrorHandler *errh)
{
    int res;
    struct ifreq ifr;
    strncpy(ifr.ifr_name, _dev->name, IFNAMSIZ);
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;

    mm_segment_t oldfs = get_fs();
    set_fs(get_ds());

    ifr.ifr_hwaddr.sa_family = _dev->type;
    memcpy(ifr.ifr_hwaddr.sa_data, _macaddr.data(), 6);
    if ((res = dev_ioctl(SIOCSIFHWADDR, &ifr)) < 0)
	errh->error("error %d setting hardware address for device `%s'", res, _devname.cc());

    sin->sin_family = AF_INET;
    sin->sin_addr = _destaddr;
    if (res >= 0 && (res = devinet_ioctl(SIOCSIFADDR, &ifr)) < 0)
	errh->error("error %d setting address for device `%s'", res, _devname.cc());

    sin->sin_addr = _destmask;
    if (res >= 0 && (res = devinet_ioctl(SIOCSIFNETMASK, &ifr)) < 0)
	errh->error("error %d setting netmask for device `%s'", res, _devname.cc());

    set_fs(oldfs);
    return res;
}

static int
dev_updown(net_device *dev, int up, ErrorHandler *errh)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, dev->name, IFNAMSIZ);
    uint32_t flags = IFF_UP | IFF_RUNNING;
    int res;

    mm_segment_t oldfs = get_fs();
    set_fs(get_ds());

    (void) dev_ioctl(SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags = (up > 0 ? ifr.ifr_flags | flags : ifr.ifr_flags & ~flags);
    if ((res = dev_ioctl(SIOCSIFFLAGS, &ifr)) < 0 && errh)
	errh->error("error %d bringing %s device `%s'", res, (up > 0 ? "up" : "down"), dev->name);

    set_fs(oldfs);
    return res;
}

int
FromLinux::initialize(ErrorHandler *errh)
{
    int res;
    if (_dev->flags & IFF_UP) {
	_wakeup_timer.initialize(this);
	_wakeup_timer.schedule_now();
	res = 0;
    } else {
	res = set_device_addresses(errh);
	if (res >= 0)
	    res = dev_updown(_dev, 1, errh);
    }

    if (res < 0) {
	uninitialize();
	return res;
    } else {
	fromlinux_map.insert(this);
	return 0;
    }
}

void
FromLinux::uninitialize()
{
    fromlinux_map.remove(this);
    if (_dev) {
	dev_put(_dev);
	uint32_t count = (uint32_t)(_dev->priv);
	_dev->priv = (void *)(count - 1);
	if (count > 1) {
	    _dev = 0;
	    return;		// do not free device; still in use
	}

	if (_dev->flags & IFF_UP)
	    dev_updown(_dev, -1, 0);
	unregister_netdev(_dev);
	kfree(_dev);
	_dev = 0;
    }
}

static void
fl_wakeup(Timer *, void *thunk)
{
    FromLinux *fl = (FromLinux *)thunk;
    PrefixErrorHandler errh(ErrorHandler::default_handler(), fl->declaration() + ": ");
    net_device *dev = fl->device();

    if (dev->flags & IFF_UP)
	dev_updown(dev, -1, &errh);
    
    fl->set_device_addresses(&errh);
    
    dev_updown(dev, 1, &errh);
}

/*
 * Device callbacks
 */

static int
fl_open(net_device *dev)
{
    netif_start_queue(dev);
    return 0;
}

static int
fl_close(net_device *dev)
{
    netif_stop_queue(dev);
    return 0;
}

static int
fl_tx(struct sk_buff *skb, net_device *dev)
{
    if (FromLinux *fl = (FromLinux *)fromlinux_map.lookup(dev, 0)) {
	Packet *p = Packet::make(skb);
	fl->push(0, p);
	return 0;
    }
    return -1;
}

static net_device_stats *
fl_stats(net_device *dev)
{
    if (FromLinux *fl = (FromLinux *)fromlinux_map.lookup(dev, 0))
	return fl->stats();
    return 0;
}

/*
 * Routing table management
 */

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromLinux)

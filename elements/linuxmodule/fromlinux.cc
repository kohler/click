/*
 * fromlinux.{cc,hh} -- receives packets from Linux
 * Max Poletto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/router.hh>
#include "fromlinux.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/etheraddress.hh>

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

static int iff_set(struct ifreq *ifr, short flag);
static int iff_clear(struct ifreq *ifr, short flag);
static int fl_init(net_device *dev);

static AnyDeviceMap fromlinux_map;
static int from_linux_count;

static void
fromlinux_static_initialize()
{
  from_linux_count++;
  if (from_linux_count > 1) return;
  fromlinux_map.initialize();  
}

static void
fromlinux_static_cleanup()
{
  from_linux_count--;
}

FromLinux::FromLinux()
{
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  _dev = 0;
  fromlinux_static_initialize();
  add_output();
}

FromLinux::~FromLinux()
{
  // no MOD_DEC_USE_COUNT; rely on AnyDevice
  fromlinux_static_cleanup();
}

FromLinux *
FromLinux::clone() const
{
  return new FromLinux();
}

int
FromLinux::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  EtherAddress ea((unsigned char *)"\000\001\002\003\004\005");
  int res = cp_va_parse(conf, this, errh,
			cpString, "interface name", &_devname,
			cpIPPrefix, "destination IP prefix", &_destaddr, &_destmask,
			cpKeywords,
			"ETHER", cpEthernetAddress, "fake device Ethernet address", &ea,
			cpEnd);
  if (res < 0)
    return res;
  _devname = _devname.substring(0, IFNAMSIZ-1);
  return initialize_device(errh, ea);
}

int
FromLinux::init_dev()
{
  _dev = new net_device;
  if (!_dev)
    goto bad;
  memset(_dev, 0, sizeof(net_device));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
  _dev->name = new char[IFNAMSIZ];
  if (!_dev->name)
    goto bad;
#endif
  strncpy(_dev->name, _devname.cc(), IFNAMSIZ);
  _dev->init = fl_init;
  memset(&_stats, 0, sizeof(net_device_stats));
  return 0;

 bad:
  if (_dev && _dev->name)
    delete[] _dev->name;
  delete _dev;
  _dev = 0L;
  return -ENOMEM;
}

int
FromLinux::initialize_device(ErrorHandler *errh, const EtherAddress &mac_addr)
{
  int res = 0;
  _device_up = false;
  
  _dev = dev_get_by_name(_devname.cc());
  if (_dev) {
      fromlinux_map.insert(this);
      return 0;
  }

  // Install fake interface
  if ((res = init_dev()) < 0)
    errh->error("error %d initializing device %s", res, _devname.cc());
  if (res >= 0 && (res = register_netdev(_dev)) < 0)
    errh->error("error %d registering device %s", res, _devname.cc());

  // Bring up the interface
  struct ifreq ifr;
  strncpy(ifr.ifr_name, _dev->name, IFNAMSIZ);

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());

  ifr.ifr_hwaddr.sa_family = _dev->type;
  memcpy(ifr.ifr_hwaddr.sa_data, mac_addr.data(), 6);
  if (res >= 0 && (res = dev_ioctl(SIOCSIFHWADDR, &ifr)) < 0)
    errh->error("error %d setting hardware address for interface %s", res, _devname.cc());
  
  struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
  sin->sin_family = AF_INET;
  sin->sin_addr = _destaddr;
  if (res >= 0 && (res = devinet_ioctl(SIOCSIFADDR, &ifr)) < 0)
    errh->error("error %d setting address for interface %s", res, _devname.cc());
  
  sin->sin_addr = _destmask;
  if (res >= 0 && (res = devinet_ioctl(SIOCSIFNETMASK, &ifr)) < 0)
    errh->error("error %d setting netmask for interface %s", res, _devname.cc());

  if (res >= 0 && (res = iff_set(&ifr, IFF_UP | IFF_RUNNING)) < 0)
    errh->error("error %d bringing up interface %s", res, _devname.cc());

  if (res >= 0) {
    _device_up = true;
    fromlinux_map.insert(this);
  }

  set_fs(oldfs);

  if (res < 0)
    uninitialize();
  return res;
}

void
FromLinux::uninitialize()
{
  fromlinux_map.remove(this);
  if (fromlinux_map.lookup(_dev) != 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    dev_put(_dev);
#endif
    _dev = 0; 
    return;
  }

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());
  int res;

  // Remove the interface
  if (_device_up) {
    struct ifreq ifr;
    strncpy(ifr.ifr_name, _devname.cc(), IFNAMSIZ);
    if ((res = iff_clear(&ifr, IFF_UP)) < 0)
      click_chatter("FromLinux(%s): error %d bringing down interface", _devname.cc(), res);
  }

  set_fs(oldfs);

  // Uninstall fake interface
  if (_dev) {
    unregister_netdev(_dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
    if (_dev->name)
      delete[] _dev->name;
#endif
    delete _dev;
    _dev = 0;
  }
}

int
FromLinux::initialize(ErrorHandler *errh)
{
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (FromLinux *fl=(FromLinux *)(e->cast("FromLinux"))) {
      if (fl->ifindex() == ifindex())
	return errh->error("duplicate FromLinux for `%s'", _devname.cc());
    }
  }
  return 0;
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
    if (FromLinux *fl = (FromLinux *)fromlinux_map.lookup(dev)) {
	Packet *p = Packet::make(skb);
	fl->push(0, p);
	return 0;
    }
    return -1;
}

static net_device_stats *
fl_stats(net_device *dev)
{
    if (FromLinux *fl = (FromLinux *)fromlinux_map.lookup(dev))
	return fl->stats();
    return 0L;
}

static int
fl_init(net_device *dev)
{
  ether_setup(dev);

  dev->open            = fl_open;
  dev->stop            = fl_close;
  dev->hard_start_xmit = fl_tx;
  dev->get_stats       = fl_stats;

  return 0;
}

/*
 * Interface flags
 */

static int
iff_set(struct ifreq *ifr, short flag)
{
  int res;
  if ((res = dev_ioctl(SIOCGIFFLAGS, ifr)) < 0)
    return res;
  ifr->ifr_flags |= flag;
  res = dev_ioctl(SIOCSIFFLAGS, ifr);
  return res;
}

static int
iff_clear(struct ifreq *ifr, short flag)
{
  int res;
  if ((res = dev_ioctl(SIOCGIFFLAGS, ifr)) < 0)
    return res;
  ifr->ifr_flags &= ~flag;
  res = dev_ioctl(SIOCSIFFLAGS, ifr);
  return res;
}

/*
 * Routing table management
 */

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromLinux)

/*
 * fromlinux.{cc,hh} -- receives packets from Linux
 * Max Poletto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/router.hh>
#include "fromlinux.hh"
#include <click/confparse.hh>
#include <click/error.hh>
extern "C" {
#define new xxx_new
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <net/route.h>
#undef new
}

static int iff_set(struct ifreq *ifr, short flag);
static int iff_clear(struct ifreq *ifr, short flag);
static int fl_init(struct device *dev);

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
  _dev = 0; _rt = 0;
  fromlinux_static_initialize();
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
  int res = cp_va_parse(conf, this, errh,
			cpString, "interface name", &_devname,
			cpIPPrefix, "destination IP prefix", &_destaddr, &_destmask,
			cpEnd);
  if (res < 0)
    return res;
  _devname = _devname.substring(0, IFNAMSIZ-1);
  return initialize_device(errh);
}

int
FromLinux::init_dev()
{
  _dev = new struct device;
  if (!_dev)
    goto bad;
  memset(_dev, 0, sizeof(struct device));
  _dev->name = new char[IFNAMSIZ];
  if (!_dev->name)
    goto bad;
  strncpy(_dev->name, _devname.cc(), IFNAMSIZ);
  _dev->init = fl_init;
  return 0;

 bad:
  if (_dev && _dev->name)
    delete[] _dev->name;
  delete _dev;
  _dev = 0L;
  return -ENOMEM;
}

int
FromLinux::init_rt(void)
{
  union {
       struct sockaddr_in m;
       struct sockaddr d;
  } s;

  _rt = new struct rtentry;
  if (!_rt)
    return -ENOMEM;
  memset(_rt, 0, sizeof(struct rtentry));

  s.m.sin_family = AF_INET;
  s.m.sin_addr.s_addr = _destaddr.addr() & _destmask.addr();
  _rt->rt_dst = s.d;
  s.m.sin_addr.s_addr = _destmask.addr();
  _rt->rt_genmask = s.d;
  _rt->rt_flags = RTF_UP;
  _rt->rt_dev = _dev->name;
  return 0;
}

#define CLEAN_RT  { delete _rt; _rt = 0; }
#define CLEAN_DEV { if (_dev->name) delete[] _dev->name; \
		    delete _dev; _dev = 0; }
#define UNREG_DEV { unregister_netdev(_dev); }
#define IF_DOWN { \
  struct ifreq ifr; \
  strncpy(ifr.ifr_name, _devname.cc(), IFNAMSIZ); \
  if ((res = iff_clear(&ifr, IFF_UP)) < 0) \
    click_chatter("FromLinux(%s): error %d bringing down interface\n", _devname.cc(), res); \
}
#define DEL_RT { \
  if ((res = ip_rt_ioctl(SIOCDELRT, _rt)) < 0) \
    click_chatter("FromLinux(%s): error %d removing route\n", _devname.cc(), res); \
}

int
FromLinux::initialize_device(ErrorHandler *errh)
{
  int res;
  
  _dev = dev_get(_devname.cc());
  if (_dev) {
    _rt = (struct rtentry *) _dev->priv;
    if (fromlinux_map.insert(this) < 0) 
      return errh->error("cannot use FromLinux for device `%s'",_devname.cc());
    return 0;
  }

  // Install fake interface
  if ((res = init_dev()) < 0)
    return errh->error("error %d initializing device %s",
		         res, _devname.cc());
  if ((res = register_netdev(_dev)) < 0) {
    CLEAN_DEV;
    return errh->error("error %d registering device %s",
		         res, _devname.cc());
  }

  // Bring up the interface
  struct ifreq ifr;
  strncpy(ifr.ifr_name, _dev->name, IFNAMSIZ);
  struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
  sin->sin_family = AF_INET;
  sin->sin_addr = _destaddr;

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());

  if ((res = devinet_ioctl(SIOCSIFADDR, &ifr)) < 0) {
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV;
    return errh->error("error %d setting address for interface %s",
		         res, _devname.cc());
  }
  
  sin->sin_addr = _destmask;
  if ((res = devinet_ioctl(SIOCSIFNETMASK, &ifr)) < 0) {
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV;
    return errh->error("error %d setting netmask for interface %s",
		         res, _devname.cc());
  }
  
  if ((res = iff_set(&ifr, IFF_UP|IFF_RUNNING)) < 0) {
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV;
    return errh->error("error %d bringing up interface %s", 
		         res, _devname.cc());
  }
				  // Establish the route
  if ((res = init_rt()) < 0) {
    IF_DOWN;
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV;
    return errh->error("error %d initializing route", res);
  }
  if ((res = ip_rt_ioctl(SIOCADDRT, _rt)) < 0) {
    CLEAN_RT;
    IF_DOWN;
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV; 
    return errh->error("error %d establishing route", res);
  }
  _dev->priv = _rt;
    
  if (fromlinux_map.insert(this) < 0) { 
    DEL_RT;
    CLEAN_RT;
    IF_DOWN;
    set_fs(oldfs);
    UNREG_DEV;
    CLEAN_DEV; 
    return errh->error("cannot use FromLinux for device `%s'",_devname.cc());
  }

  set_fs(oldfs);

  memset(&_stats, 0, sizeof(_stats));
  return res;
}

void
FromLinux::uninitialize()
{
  int res;

  fromlinux_map.remove(this);
  
  if (fromlinux_map.lookup(ifindex()) != 0) {
    _dev = 0; 
    _rt = 0;
    return;
  }

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());

  // Remove the route
  if (_rt) {
    DEL_RT;
    CLEAN_RT;
  }

  IF_DOWN;
  set_fs(oldfs);

  // Uninstall fake interface
  UNREG_DEV;
  CLEAN_DEV;
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
fl_open(struct device *dev)
{
  // set to an arbitrary Ethernet address
  for (int i = 0; i < ETH_ALEN; i++)
    dev->dev_addr[i] = i;
  dev->start = 1;
  dev->tbusy = 0;
  return 0;
}

static int
fl_close(struct device *dev)
{
  dev->start = 0;
  dev->tbusy = 1;
  return 0;
}

static int
fl_tx(struct sk_buff *skb, struct device *dev)
{
  FromLinux *fl;

  if (dev->ifindex >= 0 && dev->ifindex < MAX_DEVICES) 
    if (fl = (FromLinux*)fromlinux_map.lookup(dev->ifindex)) {
      Packet *p = Packet::make(skb);
      fl->push(0, p);
      return 0;
    }
  return -1;
}

static enet_statistics *
fl_stats(struct device *dev)
{
  FromLinux *fl;

  if (dev->ifindex >= 0 && dev->ifindex < MAX_DEVICES) 
    if (fl = (FromLinux*)fromlinux_map.lookup(dev->ifindex))
      return fl->stats();
  return 0L;
}

static int
fl_init(struct device *dev)
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

/*
 * fromlinux.{cc,hh} -- receives packets from Linux
 * Max Poletto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "fromlinux.hh"
#include "confparse.hh"
#include "error.hh"

static int iff_set(struct ifreq *ifr, short flag);
static int iff_clear(struct ifreq *ifr, short flag);
static int fl_init(struct device *dev);

FromLinux::FromLinux()
{
  _dev = 0; _rt = 0;
  add_output();
}

FromLinux::~FromLinux()
{
  uninitialize();
}

FromLinux *
FromLinux::clone() const
{
  return new FromLinux();
}

int
FromLinux::configure(const String &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpString, "interface name", &_devname,
			cpIPAddress, "destination IP address", &_destaddr,
			cpIPAddress, "destination netmask", &_destmask,
			cpEnd);
  if (res < 0)
    return res;
  _devname = _devname.substring(0, IFNAMSIZ-1);
  return res;
}

int
FromLinux::init_dev(void)
{
  if (!(_dev = kmalloc(sizeof(struct device), GFP_KERNEL)))
    return -ENOMEM;
  memset(_dev, 0, sizeof(struct device));
  if (!(_dev->name = kmalloc(IFNAMSIZ, GFP_KERNEL)))
    return -ENOMEM;
  strncpy(_dev->name, _devname.cc(), IFNAMSIZ);
  _dev->init = fl_init;
  if (!(_dev->priv = kmalloc(sizeof(struct fl_priv), GFP_KERNEL)))
    return -ENOMEM;
  memset(_dev->priv, 0, sizeof(struct fl_priv));
  ((struct fl_priv *)(_dev->priv))->fl = this;
  return 0;
}

int
FromLinux::init_rt(void)
{
  union {
       struct sockaddr_in m;
       struct sockaddr d;
  } s;

  if (!(_rt = kmalloc(sizeof(struct rtentry), GFP_KERNEL)))
    return -ENOMEM;
  memset(_rt, 0, sizeof(struct rtentry));

  s.m.sin_family = AF_INET;
  s.m.sin_addr.s_addr = _destaddr.saddr();
  _rt->rt_dst = s.d;
  s.m.sin_addr.s_addr = _destmask.saddr();
  _rt->rt_genmask = s.d;
  _rt->rt_flags = RTF_UP;
  _rt->rt_dev = _dev->name;
  return 0;
}

int
FromLinux::initialize(ErrorHandler *errh)
{
  int res;
				// Install fake interface
  if ((res = init_dev()) < 0)
    return errh->error("fromlinux: error %d initializing device %s\n", 
		       res, _devname.cc());
  if ((res = register_netdev(_dev)) < 0)
    return errh->error("fromlinux: error %d registering device %s\n", 
		       res, _devname.cc());
				// Bring up the interface
  struct ifreq ifr;
  strncpy(ifr.ifr_name, _dev->name, IFNAMSIZ);
  struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = 0;

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());

  if ((res = devinet_ioctl(SIOCSIFADDR, &ifr)) < 0) {
    set_fs(oldfs);
    return errh->error("fromlinux: error %d setting address for interface "
		       "%s\n", res, _devname.cc());
  }
  if ((res = iff_set(&ifr, IFF_UP|IFF_RUNNING)) < 0) {
    set_fs(oldfs);
    return errh->error("fromlinux: error %d bringing up interface %s\n", 
		       res, _devname.cc());
  }
				// Establish the route
  if ((res = init_rt()) < 0) {
    set_fs(oldfs);
    return errh->error("fromlinux: error %d initializing route\n", res);
  }
  if ((res = ip_rt_ioctl(SIOCADDRT, _rt)) < 0) {
    set_fs(oldfs);
    return errh->error("fromlinux: error %d establishing route\n", res);
  }

  set_fs(oldfs);
  return res;
}

void
FromLinux::uninitialize(void)
{
  int res;

  if (!_dev)
    return;

  mm_segment_t oldfs = get_fs();
  set_fs(get_ds());

				// Remove the route
  if (_rt) {
    if ((res = ip_rt_ioctl(SIOCDELRT, _rt)) < 0) {
      click_chatter("fromlinux: error %d removing route\n", res);
    }
    kfree(_rt);
    _rt = 0;
  }
				// Bring down the interface
  struct ifreq ifr;
  strncpy(ifr.ifr_name, _devname.cc(), IFNAMSIZ);
  if ((res = iff_clear(&ifr, IFF_UP)) < 0)
    click_chatter("fromlinux: error %d bringing down interface %s\n", 
		  res, _devname.cc());
  set_fs(oldfs);
				// Uninstall fake interface
  if (_dev->priv)
    kfree(_dev->priv);
  unregister_netdev(_dev);
  kfree(_dev);
  _dev = 0;
}

/*
 * Device callbacks
 */

static int
fl_open(struct device *dev)
{
  for (int i = 0; i < ETH_ALEN; i++)
    dev->dev_addr[i] = 0;
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
  struct fl_priv *priv = (struct fl_priv *) dev->priv;
  FromLinux *t = priv->fl;
  Packet *p = Packet::make(skb);
  t->push(0, p);
  return 0;
}

static struct enet_statistics *
fl_stats(struct device *dev)
{
  struct fl_priv *priv = (struct fl_priv *) dev->priv;
  return &priv->stats;
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


EXPORT_ELEMENT(FromLinux)

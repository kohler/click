// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromhost.{cc,hh} -- receives packets from Linux
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2009 Meraki, Inc.
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
#include "fromhost.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/ip6.h>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <net/route.h>
#include <net/dst.h>
#include <net/inet_common.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# include <net/net_namespace.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if HAVE_LINUX_INET_CTL_SOCK_CREATE
# define click_inet_ioctl(sock, dev, cmd, arg)	inet_ioctl((sock), (cmd), (arg))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
# define click_inet_ioctl(sock, dev, cmd, arg)	devinet_ioctl(dev_net(dev), (cmd), (arg))
#else
# define click_inet_ioctl(sock, dev, cmd, arg)	devinet_ioctl((cmd), (arg))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
# define click_dev_ioctl(dev, cmd, arg)		dev_ioctl(dev_net(dev), (cmd), (arg))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# define click_dev_ioctl(dev, cmd, arg)		dev_ioctl(&init_net, (cmd), (arg))
#else
# define click_dev_ioctl(dev, cmd, arg)		dev_ioctl((cmd), (arg))
#endif

#ifndef NETDEV_TX_OK
# define NETDEV_TX_OK		0
# define NETDEV_TX_BUSY		1
#endif

extern "C" {
static int fl_open(net_device *);
static int fl_close(net_device *);
static void fl_wakeup(Timer *, void *);
}

FromHost *FromHost::configuring = 0;
static AnyDeviceMap fromlinux_map;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
static struct net_device_ops fromhost_netdev_ops;
# define netdev_op(dev, opname)		(dev)->netdev_ops->ndo_##opname
#else
# define netdev_op(dev, opname)		(dev)->opname
#endif

void
FromHost::static_initialize()
{
    fromlinux_map.initialize();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    fromhost_netdev_ops.ndo_open = fl_open;
    fromhost_netdev_ops.ndo_stop = fl_close;
    fromhost_netdev_ops.ndo_start_xmit = fl_tx;
    fromhost_netdev_ops.ndo_set_mac_address = eth_mac_addr;
#endif
}

FromHost::FromHost()
    : _macaddr((const unsigned char *)"\000\001\002\003\004\005"),
      _task(this), _wakeup_timer(fl_wakeup, this),
      _drops(0), _ninvalid(0)
{
    set_head(0);
    set_tail(0);
    _capacity = 100;
    _q.lgq = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    memset(&_stats, 0, sizeof(_stats));
#endif
}

FromHost::~FromHost()
{
}

void *FromHost::cast(const char *name)
{
    if (strcmp(name, "Storage") == 0)
	return (Storage *)this;
    else if (strcmp(name, "FromHost") == 0)
	return (Element *)this;
    else
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
extern "C" {
static void fromhost_inet_setup(struct net_device *dev)
{
    dev->type = ARPHRD_NONE;
    dev->hard_header_len = 0;
    dev->addr_len = 0;
    dev->mtu = 1500;
    dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
}
}
#endif

/* include/uapi/linux/netdevice.h */
/* interface name assignment types (sysfs name_assign_type attribute) */
/* #define NET_NAME_UNKNOWN	0	 unknown origin (not exposed to userspace) */
/* #define NET_NAME_ENUM	1	 enumerated by kernel */
/* #define NET_NAME_PREDICTABLE	2	 predictably named by the kernel */
/* #define NET_NAME_USER	3	 provided by user-space */
/* #define NET_NAME_RENAMED	4	 renamed by user-space */
net_device *
FromHost::new_device(const char *name)
{
    void (*setup)(struct net_device *) = (_macaddr ? ether_setup : fromhost_inet_setup);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    net_device *dev = alloc_netdev(0, name, NET_NAME_UNKNOWN, setup);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    net_device *dev = alloc_netdev(0, name, setup);
#else
    int errcode;
    read_lock(&dev_base_lock);
    net_device *dev = dev_alloc(name, &errcode);
    read_unlock(&dev_base_lock);
#endif
    if (!dev)
	return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    dev->netdev_ops = &fromhost_netdev_ops;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    dev->open = fl_open;
    dev->stop = fl_close;
    dev->hard_start_xmit = fl_tx;
    dev->get_stats = fl_stats;
#else
    setup(dev);
#endif
    dev->mtu = _mtu;
    dev->tx_queue_len = 0;
    dev->ifindex = -1;
    return dev;
}

int
FromHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String type;
    int mtu = 1500;
    _destaddr = IPAddress();
    _destmask = IPAddress();
    _clear_anno = true;
    _burst = 8;

    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _devname)
	.read_p("PREFIX", IPPrefixArg(), _destaddr, _destmask)
	.read("TYPE", WordArg(), type)
	.read("ETHER", _macaddr)
	.read("MTU", mtu)
	.read("CAPACITY", _capacity)
	.read("CLEAR_ANNO", _clear_anno)
	.read("BURST", BoundedIntArg(1, 1000000), _burst)
	.complete() < 0)
	return -1;

    // check for duplicate element
    if (_devname.length() > IFNAMSIZ - 1)
	return errh->error("device name '%s' too long", _devname.c_str());
    void *&used = router()->force_attachment("FromHost_" + _devname);
    if (used)
	return errh->error("duplicate FromHost for device '%s'", _devname.c_str());
    used = this;

    _mtu = mtu;
    // check for existing device
    _dev = AnyDevice::get_by_name(_devname.c_str());
    if (_dev) {
	if (netdev_op(_dev, open) != fl_open) {
	    dev_put(_dev);
	    _dev = 0;
	    return errh->error("device '%s' already exists", _devname.c_str());
	} else {
	    fromlinux_map.insert(this, false);
	    return 0;
	}
    }

    // set type
    if (type == "IP")
	_macaddr = EtherAddress();
    else if (type != "ETHER" && type != "")
	return errh->error("bad TYPE");

    // set up queue
    if (_capacity < 1)
	_capacity = 1;
    if (_capacity > smq_size)
	if (!(_q.lgq = new Packet *[_capacity + 1]))
	    return errh->error("out of memory!");

    // if not found, create new device
    int res;
    _dev = new_device(_devname.c_str());
    if (!_dev)
	return errh->error("out of memory registering device '%s'", _devname.c_str());
    else {
	// in Linux < 2.6.32 register_netdev calls copy_rtnl_link_stats which
	// requires valid net_device_stats structure, so remember which
	// FromHost we're configuring (configure is serialized)
	dev_hold(_dev);
	configuring = this;
	res = register_netdev(_dev);
	configuring = 0;
	if (res == 0)
	    fromlinux_map.insert(this, false);
	else {
	    dev_put(_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	    free_netdev(_dev);
#else
	    kfree(_dev);
#endif
	    _dev = 0;
	    return errh->error("error %d registering device '%s'", res, _devname.c_str());
	}
    }

    return 0;
}

int
FromHost::set_device_addresses(ErrorHandler *errh)
{
    int res = 0;
    struct ifreq ifr;
    strncpy(ifr.ifr_name, _dev->name, IFNAMSIZ);
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    mm_segment_t oldfs;

    if (_macaddr) {
	ifr.ifr_hwaddr.sa_family = _dev->type;
	memcpy(ifr.ifr_hwaddr.sa_data, _macaddr.data(), 6);
#if HAVE_LINUX_DEV_IOCTL
	oldfs = get_fs();
	set_fs(get_ds());
	res = click_dev_ioctl(_dev, SIOCSIFHWADDR, &ifr);
	set_fs(oldfs);
#else
	rtnl_lock();
	res = dev_set_mac_address(_dev, &ifr.ifr_hwaddr);
	rtnl_unlock();
#endif
	if (res)
	    errh->error("error %d setting hardware address for device '%s'",
			res, _devname.c_str());
    }

    if (_destaddr) {
#if HAVE_LINUX_DEVINET_IOCTL || HAVE_LINUX_INET_CTL_SOCK_CREATE
        sin->sin_family = AF_INET;
        sin->sin_addr = _destaddr;
# if HAVE_LINUX_INET_CTL_SOCK_CREATE
	struct socket *sock = (struct socket*) kmalloc(sizeof(struct socket), GFP_KERNEL);
	sock->sk = 0;
	if (res >= 0 && (res = inet_ctl_sock_create(&sock->sk, AF_INET, SOCK_RAW, IPPROTO_TCP, dev_net(_dev))) != 0) {
	    errh->error("error %d creating control socket for device '%s'", res, _devname.c_str());
	    res = -1;
	}
# endif
	oldfs = get_fs();
	set_fs(get_ds());
        if (res >= 0 && (res = click_inet_ioctl(sock, _dev, SIOCSIFADDR, &ifr)) < 0)
            errh->error("error %d setting address for device '%s'", res, _devname.c_str());

        sin->sin_addr = _destmask;
        if (res >= 0 && (res = click_inet_ioctl(sock, _dev, SIOCSIFNETMASK, &ifr)) < 0)
            errh->error("error %d setting netmask for device '%s'", res, _devname.c_str());
	set_fs(oldfs);
# if HAVE_LINUX_INET_CTL_SOCK_CREATE
	inet_ctl_sock_destroy(sock->sk);
	kfree(sock);
# endif
#else
	res = errh->error("cannot set IP address for FromHost devices on this kernel");
#endif
    }

    return res;
}

static int
dev_updown(net_device *dev, int up, ErrorHandler *errh)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, dev->name, IFNAMSIZ);
    uint32_t flags = IFF_UP | IFF_RUNNING;
    int res = -EINVAL;

#if HAVE_LINUX_DEV_IOCTL
    mm_segment_t oldfs = get_fs();
    set_fs(get_ds());

    (void) click_dev_ioctl(dev, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags = (up > 0 ? ifr.ifr_flags | flags : ifr.ifr_flags & ~flags);
    res = click_dev_ioctl(dev, SIOCSIFFLAGS, &ifr);
    set_fs(oldfs);
#else
    rtnl_lock();
    res = dev_change_flags(dev, dev->flags | IFF_UP);
    rtnl_unlock();
#endif

    if (res < 0 && errh)
	errh->error("error %d bringing %s device '%s'", res, (up > 0 ? "up" : "down"), dev->name);
    return res;
}

int
FromHost::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
    _nonfull_signal = Notifier::downstream_full_signal(this, 0, &_task);
    if (_dev->flags & IFF_UP) {
	_wakeup_timer.initialize(this);
	_wakeup_timer.schedule_now();
	return 0;
    } else if (set_device_addresses(errh) < 0)
	return -1;
    else
	return dev_updown(_dev, 1, errh);
}

void
FromHost::cleanup(CleanupStage)
{
    fromlinux_map.remove(this, false);

    Packet * volatile *q = queue();
    while (head() != tail()) {
	Packet *p = q[head()];
	p->kill();
	set_head(next_i(head()));
    }
    if (_capacity > smq_size)
	delete[] _q.lgq;
    _capacity = 1;
    set_head(0);
    set_tail(0);

    if (_dev) {
	dev_put(_dev);
	unsigned long lock_flags;
	fromlinux_map.lock(false, lock_flags);
	if (fromlinux_map.lookup(_dev, 0))
	    // do not free device if it is in use
	    _dev = 0;
	fromlinux_map.unlock(false, lock_flags);
	if (_dev) {
	    if (_dev->flags & IFF_UP)
		dev_updown(_dev, -1, 0);
	    unregister_netdev(_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	    free_netdev(_dev);
#else
	    kfree(_dev);
#endif
	    _dev = 0;
	}
    }
}

/*
 * Device callbacks
 */

extern "C" {
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

static void
fl_wakeup(Timer *, void *thunk)
{
    FromHost *fl = (FromHost *)thunk;
    PrefixErrorHandler errh(ErrorHandler::default_handler(), fl->declaration() + ": ");
    net_device *dev = fl->device();

    if (dev->flags & IFF_UP)
	dev_updown(dev, -1, &errh);

    fl->set_device_addresses(&errh);

    dev_updown(dev, 1, &errh);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
net_device_stats *
FromHost::fl_stats(net_device *dev)
{
    unsigned long lock_flags;
    fromlinux_map.lock(false, lock_flags);
    FromHost *fl = (FromHost *) fromlinux_map.lookup(dev, 0);
    if (!fl)
	fl = (FromHost *) fromlinux_map.lookup_unknown(dev, 0);
    if (!fl && configuring && configuring->device() == dev)
	fl = configuring;
    net_device_stats *stats = (fl ? fl->stats() : 0);
    fromlinux_map.unlock(false, lock_flags);
    return stats;
}
#endif
}

netdev_tx_t
FromHost::fl_tx(struct sk_buff *skb, net_device *dev)
{
    /* 8.May.2003 - Doug and company had crashes with FromHost configurations.
         We eventually figured out this was because fl_tx was called at
         interrupt time -- at bottom-half time, to be exact -- and then pushed
         a packet through the configuration. Whoops: if Click was interrupted,
         and during the bottom-half FromHost emitted a packet into Click,
         DISASTER -- we assume that, when running single-threaded, at most one
         Click thread is active at a time; so there were race conditions,
         particularly with the task list. The solution is a queue in
         FromHost. fl_tx puts a packet onto the queue, a regular Click Task
         takes the packet off the queue. */
    netdev_tx_t ret;
    unsigned long lock_flags;
    fromlinux_map.lock(false, lock_flags);
    if (FromHost *fl = (FromHost *)fromlinux_map.lookup(dev, 0)) {
        Storage::index_type t = fl->tail(), nt = fl->next_i(t);
	if (likely(nt != fl->head())) {
	    Packet * volatile *q = fl->queue();

	    // skb->dst may be set since the packet came from Linux.  Since
	    // Click doesn't use dst, clear it now.
#if HAVE_SKB_DST_DROP
	    skb_dst_drop(skb);
#else
	    if (skb->dst) {
		dst_release(skb->dst);
		skb->dst = 0;
	    }
#endif

	    Packet *p = Packet::make(skb);
	    p->timestamp_anno().assign_now();
	    if (fl->_clear_anno)
		p->clear_annotations(false);
	    fl->stats()->tx_packets++;
	    fl->stats()->tx_bytes += p->length();
	    fl->_task.reschedule();
	    q[t] = p;
	    fl->set_tail(nt);
	    ret = (netdev_tx_t) NETDEV_TX_OK;
	} else {
	    fl->_drops++;
	    ret = (netdev_tx_t) NETDEV_TX_BUSY;	// Linux will free the packet.
	}
    } else
	ret = (netdev_tx_t) NETDEV_TX_BUSY;
    fromlinux_map.unlock(false, lock_flags);
    return ret;
}

bool
FromHost::run_task(Task *)
{
    if (!_nonfull_signal || unlikely(empty()))
	return false;

    Packet * volatile *q = queue();
    for (int count = 0; count < _burst && !empty(); ++count) {
        Storage::index_type h = head();
	Packet *p = q[h];
	set_head(next_i(h));

	// Convenience for TYPE IP: set the IP header and destination address.
	if (_dev->type == ARPHRD_NONE && p->length() >= 1) {
	    const click_ip *iph = (const click_ip *) p->data();
	    if (iph->ip_v == 4) {
		if (iph->ip_hl >= 5
		    && ntohs(iph->ip_len) >= (iph->ip_hl << 2)
		    && reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2) <= p->end_data()) {
		    p->set_ip_header(iph, iph->ip_hl << 2);
		    p->set_dst_ip_anno(iph->ip_dst);
		} else
		    goto bad;
	    } else if (iph->ip_v == 6) {
		if (reinterpret_cast<const uint8_t *>(iph) + sizeof(click_ip6) <= p->end_data())
		    p->set_ip6_header(reinterpret_cast<const click_ip6 *>(iph));
		else
		    goto bad;
	    } else {
	      bad:
	        _ninvalid++;
		checked_output_push(1, p);
		continue;
	    }
	}

	output(0).push(p);
    }

    if (!empty())
        _task.fast_reschedule();
    return true;
}

String
FromHost::read_handler(Element *e, void *)
{
    FromHost *fh = (FromHost *) e;
    return String(fh->size());
}

int FromHost::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    FromHost *fh = static_cast<FromHost *>(e);
    switch (reinterpret_cast<intptr_t>(thunk)) {
    case h_burst:
        if (!BoundedIntArg(1, 1000000).parse(str, fh->_burst))
            return errh->error("burst parameter must be integer between 1 and 1000000");
	return 0;
    case h_ether: {
        EtherAddress macaddr;
        if (Args(errh).push_back_words(str)
                .read_mp("ETHER", macaddr)
                .complete() < 0)
            return -1;
        if (!macaddr)
            return errh->error("ether parameter must be set");

        if (fh->_macaddr != macaddr) {
            fh->_macaddr = macaddr;
            if (fh->_dev->flags & IFF_UP) {
                if (!fh->_wakeup_timer.initialized())
                    fh->_wakeup_timer.initialize(fh);
                fh->_wakeup_timer.schedule_now();
                return 0;
            } else if (fh->set_device_addresses(errh) < 0)
                return -1;
        }
        return 0;
    }
    default:
	return 0;
    }
}

void
FromHost::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("length", read_handler, h_length);
    add_data_handlers("capacity", Handler::OP_READ, &_capacity);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("burst", Handler::OP_READ, &_burst);
    add_write_handler("burst", write_handler, h_burst);
    if (_dev->type == ARPHRD_ETHER) {
        add_data_handlers("ether", Handler::OP_READ, &_macaddr);
        add_write_handler("ether", write_handler, h_ether);
    }
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromHost)

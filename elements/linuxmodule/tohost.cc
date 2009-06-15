// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * tohost.{cc,hh} -- element sends packets to Linux for default processing
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (C) 2003 International Computer Science Institute
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
#include "tohost.hh"
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <net/dst.h>
#include <linux/smp_lock.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#if LINUX_VERSION_CODE >= 0x020400 && LINUX_VERSION_CODE < 0x020600
# include <linux/brlock.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

// for watching when devices go offline
static AnyDeviceMap to_host_map;
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

void
ToHost::static_initialize()
{
    to_host_map.initialize();
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = 1;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);
}

void
ToHost::static_cleanup()
{
    unregister_netdevice_notifier(&device_notifier);
}

ToHost::ToHost()
    : _sniffers(false), _drops(0)
{
}

ToHost::~ToHost()
{
}

int
ToHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String type;
    if (AnyDevice::configure_keywords(conf, errh, false) < 0
	|| cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkP, cpString, &_devname,
			"SNIFFERS", 0, cpBool, &_sniffers,
			"TYPE", 0, cpWord, &type,
			cpEnd) < 0)
	return -1;
    if (type == "ETHER" || type == "")
	_type = ARPHRD_ETHER;
    else if (type == "IP")
	_type = ARPHRD_NONE;
    else
	return errh->error("bad TYPE");
    return 0;
}

int
ToHost::initialize(ErrorHandler *errh)
{
    if (AnyDevice::initialize_keywords(errh) < 0)
	return -1;

    // Avoid warnings about "device down" with FromHost devices -- FromHost
    // brings up its device during initialize().
    int r;
    if (_devname && (r = find_device(&to_host_map, errh)) < 0)
	return r;
    return 0;
}

void
ToHost::cleanup(CleanupStage)
{
    clear_device(&to_host_map);
}

extern "C" {
static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP) {
	bool exists = (flags != NETDEV_UP);
	net_device *dev = (net_device *)v;
	unsigned long lock_flags;
	to_host_map.lock(true, lock_flags);
	AnyDevice *es[8];
	int nes = to_host_map.lookup_all(dev, exists, es, 8);
	for (int i = 0; i < nes; i++)
	    ((ToHost *)(es[i]))->set_device(flags == NETDEV_DOWN ? 0 : dev, &to_host_map, true);
	to_host_map.unlock(true, lock_flags);
    }
    return 0;
}
}

void
ToHost::push(int port, Packet *p)
{
    p->clear_annotations(false);

    struct sk_buff *skb = p->skb();

    // set device if specified
    if (_dev)
	skb->dev = _dev;

    // check that device exists
    if (!skb->dev) {
	if (++_drops == 1)
	    click_chatter("%{element}: dropped a packet with null skb->dev", this);
	p->kill();
	return;
    }

    // remove PACKET_CLEAN bit -- packet is becoming dirty
    skb->pkt_type &= PACKET_TYPE_MASK;

    // MAC header is the data pointer
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    skb_set_mac_header(skb, 0);
#else
    skb->mac.raw = skb->data;
#endif

    // set skb->protocol
    if (_type == ARPHRD_NONE)
	skb->protocol = __constant_htons(ETH_P_IP);
    else {
	// do not call eth_type_trans; it changes pkt_type! Instead, do its
	// work directly.
	skb_pull(skb, 14);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	const ethhdr *eth = eth_hdr(skb);
#else
	const ethhdr *eth = skb->mac.ethernet;
#endif
	if (ntohs(eth->h_proto) >= 1536)
	    skb->protocol = eth->h_proto;
	else {
	    const unsigned short *crap = (const unsigned short *)skb->data;
	    // "magic hack to spot IPX packets"
	    skb->protocol = (*crap == 0xFFFF ? htons(ETH_P_802_3) : htons(ETH_P_802_2));
	}
    }

    // skb->dst may be set if the packet came from Linux originally. In this
    // case, we must clear skb->dst so Linux finds the correct dst.
    if (skb->dst) {
	dst_release(skb->dst);
	skb->dst = 0;
    }

    // get protocol to pass to Linux
    int protocol = (_sniffers ? 0xFFFF : skb->protocol);

    // pass packet to Linux
#ifdef HAVE_NETIF_RECEIVE_SKB	// from Linux headers
    struct net_device *dev = skb->dev;
    local_bh_disable();
    dev_hold(dev);
# if HAVE___NETIF_RECEIVE_SKB
    int ret = __netif_receive_skb(skb, protocol, -1);
# else
    netif_receive_skb(skb, protocol, -1);
# endif
    dev_put(dev);
    local_bh_enable();
#else
    // be nice to libpcap
    if (skb->stamp.tv_sec == 0) {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 18)
	do_gettimeofday(&skb->stamp);
# elif !defined(CONFIG_CPU_IS_SLOW)
	get_fast_time(&skb->stamp);
# else
	skb->stamp = xtime;
# endif
    }

# ifdef HAVE_CLICK_KERNEL
    skb->h.raw = skb->nh.raw = skb->data;
#  if LINUX_VERSION_CODE >= 0x020400
    local_bh_disable();
    br_read_lock(BR_NETPROTO_LOCK);
    struct net_device *dev = skb->dev;
    dev_hold(dev);
    ptype_dispatch(skb, protocol);
    dev_put(dev);
    br_read_unlock(BR_NETPROTO_LOCK);
    local_bh_enable();
#  else
    lock_kernel();
    ptype_dispatch(skb, protocol);
    unlock_kernel();
#  endif
# endif
#endif
}

void
ToHost::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

ELEMENT_REQUIRES(linuxmodule AnyDevice)
EXPORT_ELEMENT(ToHost)

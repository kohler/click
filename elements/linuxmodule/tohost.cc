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
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#if LINUX_VERSION_CODE >= 0x020400
# include <linux/brlock.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

// for watching when devices go offline
static AnyDeviceMap to_host_map;
static int to_host_count;
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

ToHost::ToHost()
    : _sniffers(false), _allow_nonexistent(false), _drops(0)
{
    MOD_INC_USE_COUNT;
    add_input();

    // static initialize
    if (++to_host_count == 1) {
	to_host_map.initialize();
	device_notifier.notifier_call = device_notifier_hook;
	device_notifier.priority = 1;
	device_notifier.next = 0;
	register_netdevice_notifier(&device_notifier);
    }
}

ToHost::~ToHost()
{
    MOD_DEC_USE_COUNT;

    // static cleanup
    if (--to_host_count <= 0)
	unregister_netdevice_notifier(&device_notifier);
}

ToHost *
ToHost::clone() const
{
    return new ToHost();
}

int
ToHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_parse(conf, this, errh,
		       cpOptional,
		       cpString, "device name", &_devname,
		       cpKeywords,
		       "SNIFFERS", cpBool, "send packets to sniffers only?", &_sniffers,
		       "ALLOW_NONEXISTENT", cpBool, "allow nonexistent device?", &_allow_nonexistent,
		       cpEnd);
}

int
ToHost::initialize(ErrorHandler *errh)
{
    // We find the device here, rather than in 'initialize', to avoid warnings
    // about "device down" with FromHost devices -- FromHost brings up its
    // device during initialize().
    return (_devname ? find_device(_allow_nonexistent, &to_host_map, errh) : 0);
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
	bool down = (flags == NETDEV_DOWN);
	net_device *dev = (net_device *)v;
	Vector<AnyDevice *> es;
	to_host_map.lookup_all(dev, down, es);
	lock_kernel();
	for (int i = 0; i < es.size(); i++)
	    ((ToHost *)(es[i]))->set_device((down ? 0 : dev), &to_host_map);
	unlock_kernel();
    }
    return 0;
}
}

void
ToHost::push(int port, Packet *p)
{
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

    // do not call eth_type_trans; it changes pkt_type! Instead, do its work
    // directly.
    skb->mac.raw = skb->data;
    skb_pull(skb, 14);

    const ethhdr *eth = skb->mac.ethernet;
    if (ntohs(eth->h_proto) >= 1536)
	skb->protocol = eth->h_proto;
    else {
	const unsigned short *crap = (const unsigned short *)skb->data;
	// "magic hack to spot IPX packets"
	skb->protocol = (*crap == 0xFFFF ? htons(ETH_P_802_3) : htons(ETH_P_802_2));
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
    dev_hold(dev);
    netif_receive_skb(skb, protocol, -1);
    dev_put(dev);
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

String
ToHost::read_handler(Element *e, void *)
{
    ToHost *th = static_cast<ToHost *>(e);
    return String(th->_drops) + "\n";
}

void
ToHost::add_handlers()
{
    add_read_handler("drops", read_handler, 0);
}

ELEMENT_REQUIRES(linuxmodule AnyDevice)
EXPORT_ELEMENT(ToHost)

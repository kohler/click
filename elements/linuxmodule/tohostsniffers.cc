// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * tolinuxsniffers.{cc,hh} -- element sends packets to Linux sniffers
 * Eddie Kohler; based on tolinux.cc
 *
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
#include "tolinuxsniffers.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/linuxmodule/anydevice.hh"

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/smp_lock.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#if LINUX_VERSION_CODE >= 0x020400
# include <linux/brlock.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

ToLinuxSniffers::ToLinuxSniffers()
    : Element(1, 0), _dev(0)
{
    MOD_INC_USE_COUNT;
}

ToLinuxSniffers::~ToLinuxSniffers()
{
    uninitialize();		// might need to dev_put
    MOD_DEC_USE_COUNT;
}

ToLinuxSniffers *
ToLinuxSniffers::clone() const
{
    return new ToLinuxSniffers();
}

int
ToLinuxSniffers::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String devname;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "device name", &devname,
		  cpEnd) < 0)
    return -1;
  if (devname) {
    _dev = dev_get_by_name(devname.cc());
    if (!_dev)
      _dev = dev_get_by_ether_address(devname, this);
    if (!_dev)
      return errh->error("unknown device `%s'", devname.cc());
  } else
    _dev = 0;
  return 0;
}

void
ToLinuxSniffers::uninitialize()
{
    if (_dev)
	dev_put(_dev);
    _dev = 0;
}

void
ToLinuxSniffers::push(int port, Packet *p)
{
  struct sk_buff *skb = p->steal_skb();
  if (!skb) return;

  if (_dev) 
    skb->dev = _dev;

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

  // be nice to libpcap
  if (skb->stamp.tv_sec == 0) {
#ifndef CONFIG_CPU_IS_SLOW
    get_fast_time(&skb->stamp);
#else
    skb->stamp = xtime;
#endif
  }
  
#ifdef HAVE_CLICK_KERNEL
  skb->h.raw = skb->nh.raw = skb->data;
#if LINUX_VERSION_CODE >= 0x020400
  local_bh_disable();
  br_read_lock(BR_NETPROTO_LOCK);
  ptype_dispatch(skb, 0xFFFF);	// unlikely protocol
  br_read_unlock(BR_NETPROTO_LOCK);
  local_bh_enable();
#else
  lock_kernel();
  ptype_dispatch(skb, 0xFFFF);
  unlock_kernel();
#endif
#endif
}

ELEMENT_REQUIRES(linuxmodule AnyDevice)
EXPORT_ELEMENT(ToLinuxSniffers)

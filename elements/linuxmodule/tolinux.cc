/*
 * tolinux.{cc,hh} -- element sends packets to Linux for default processing
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include <click/package.hh>
#include "tolinux.hh"
#include <click/confparse.hh>
#include <click/error.hh>
extern "C" {
#define new xxx_new
#include <net/dst.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#undef new
}

ToLinux::ToLinux()
  : Element(1, 0)
{
  MOD_INC_USE_COUNT;
}

ToLinux::~ToLinux()
{
  MOD_DEC_USE_COUNT;
}

ToLinux *
ToLinux::clone() const
{
  return new ToLinux();
}

int
ToLinux::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String devname;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "device name", &devname,
		  cpEnd) < 0)
    return -1;
  if (devname) {
    _dev = dev_get(devname.cc());
    if (!_dev)
      _dev = find_device_by_ether_address(devname, this);
    if (!_dev)
      return errh->error("no such device `%s'", devname.cc());
  } else
    _dev = 0;
  return 0;
}

void
ToLinux::push(int port, Packet *p)
{
  struct sk_buff *skb = p->steal_skb();
  if (!skb) return;
  
  // set device if specified
  if (_dev) 
    skb->dev = _dev;

  // remove PACKET_CLEAN bit -- packet is becoming dirty
  skb->pkt_type &= PACKET_TYPE_MASK;

  if (skb->dev)
    skb->protocol = eth_type_trans(skb, skb->dev);

  // skb->dst may be set if the packet came from Linux originally. In this
  // case, we must clear skb->dst so Linux finds the correct dst.
  if (skb->dst) {
    dst_release(skb->dst);
    skb->dst = 0;
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
  skb->nh.raw = skb->data;
  skb->h.raw = 0;
  //start_bh_atomic();
  lock_kernel();
  ptype_dispatch(skb, skb->protocol);
  unlock_kernel();
  //end_bh_atomic();
#endif
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ToLinux)

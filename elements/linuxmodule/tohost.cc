/*
 * tolinux.{cc,hh} -- element sends packets to Linux for default processing
 * Robert Morris
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
#include "tolinux.hh"
#include "confparse.hh"
#include "error.hh"
extern "C" {
#include <linux/if_ether.h>
#include <linux/netdevice.h>
}

ToLinux::ToLinux()
{
  add_input();
}

#if 0
static unsigned long linux_cycles = 0;
static unsigned long linux_pkts = 0;
#endif

ToLinux::~ToLinux()
{
#if 0
  click_chatter("%d pkts in %u cycles", linux_pkts, linux_cycles);
#endif
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
  
  skb->mac.raw = skb->data;
  skb->protocol = skb->mac.ethernet->h_proto;
  /* skb->pkt_type = ???; */

  // set device if specified
  if (_dev)
    skb->dev = _dev;

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
  
  /* skip past ether header */
  skb_pull(skb, 14);
#ifdef HAVE_CLICK_KERNEL
  skb->nh.raw = skb->data;
  skb->h.raw = 0;
  start_bh_atomic();
#if 0
  unsigned long c0 = click_get_cycles();
#endif
  ptype_dispatch(skb, skb->protocol);
#if 0
  linux_cycles += click_get_cycles() - c0;
  linux_pkts ++;
#endif
  end_bh_atomic();
#endif
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ToLinux)

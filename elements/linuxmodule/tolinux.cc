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

void
ToLinux::push(int port, Packet *p)
{
  struct sk_buff *skb = p->steal_skb();
  if (!skb) return;
  
  skb->mac.raw = skb->data;
  skb->protocol = skb->mac.ethernet->h_proto;
  /* skb->pkt_type = ???; */

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

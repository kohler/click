/*
 * tolinuxsniffers.{cc,hh} -- element sends packets to Linux sniffers
 * Eddie Kohler; based on tolinux.cc
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
#include "tolinuxsniffers.hh"
extern "C" {
#include <linux/if_ether.h>
#include <linux/netdevice.h>
}

ToLinuxSniffers::ToLinuxSniffers()
{
  add_input();
}

ToLinuxSniffers::~ToLinuxSniffers()
{
}

ToLinuxSniffers *
ToLinuxSniffers::clone() const
{
  return new ToLinuxSniffers();
}

void
ToLinuxSniffers::push(int port, Packet *p)
{
  struct sk_buff *skb1 = p->steal_skb();
  if (!skb1) return;
  
  skb1->mac.raw = skb1->data;
  skb1->protocol = skb1->mac.ethernet->h_proto;
  /* skb->pkt_type = ???; */
  
  /* skip past ether header */
  skb_pull(skb1, 14);
#ifdef HAVE_CLICK_KERNEL
  skb1->nh.raw = skb1->data;
  start_bh_atomic();
  ptype_dispatch(skb1, 0xFFFF);	// an unlikely protocol number
  end_bh_atomic();
#endif
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ToLinuxSniffers)

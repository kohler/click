/*
 * tolinux.{cc,hh} -- element sends packets to Linux for default processing
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
#include <linux/if_ether.h>

ToLinux::ToLinux()
{
  add_input();
}

ToLinux::~ToLinux()
{
}

ToLinux *
ToLinux::clone() const
{
  return new ToLinux();
}

void
ToLinux::push(int port, Packet *p)
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
  ptype_dispatch(skb1, skb1->protocol);
#endif
}

EXPORT_ELEMENT(ToLinux)

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
#include "confparse.hh"
#include "error.hh"
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
    _dev = dev_get(devname.cc());
    if (!_dev)
      return errh->error("no such device `%s'", devname.cc());
  } else
    _dev = 0;
  return 0;
}

ToLinuxSniffers *
ToLinuxSniffers::clone() const
{
  return new ToLinuxSniffers();
}

void
ToLinuxSniffers::push(int port, Packet *p)
{
  struct sk_buff *skb = p->steal_skb();
  if (!skb) return;
  
  skb->mac.raw = skb->data;
  skb->protocol = skb->mac.ethernet->h_proto;
  if (_dev) skb->dev = _dev;
  /* skb->pkt_type = ???; */
  
  // be nice to libpcap
#ifndef CONFIG_CPU_IS_SLOW
  if (skb->stamp.tv_sec==0)
    get_fast_time(&skb->stamp);
#else
  skb->stamp = xtime;
#endif
  
  /* skip past ether header */
  skb_pull(skb, 14);
#ifdef HAVE_CLICK_KERNEL
  skb->nh.raw = skb->data;
  start_bh_atomic();
  ptype_dispatch(skb, 0xFFFF);	// an unlikely protocol number
  end_bh_atomic();
#endif
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ToLinuxSniffers)

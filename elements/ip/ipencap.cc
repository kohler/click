/*
 * ipencap.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris, Eddie Kohler, Alex Snoeren
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
#include "ipencap.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

IPEncap::IPEncap()
  : Element(1, 1), _ip_p(-1)
{
}

IPEncap::~IPEncap()
{
}

IPEncap *
IPEncap::clone() const
{
  return new IPEncap;
}

int
IPEncap::configure(const String &conf, ErrorHandler *errh)
{
  unsigned char ip_p_uc;
  if (cp_va_parse(conf, this, errh,
		  cpByte, "IP encapsulation protocol", &ip_p_uc,
		  cpIPAddress, "source IP address", &_ip_src,
		  cpIPAddress, "destination IP address", &_ip_dst,
		  0) < 0)
    return -1;
  _ip_p = ip_p_uc;
  return 0;
}

int
IPEncap::initialize(ErrorHandler *errh)
{
  _id = 0;
  return 0;
}

Packet *
IPEncap::simple_action(Packet *p)
{
  /* What the fuck was going on here???  -  ED*/
  p = p->push(sizeof(click_ip));
  click_ip *ip = (click_ip *) p->data();
  
  ip->ip_v = IPVERSION;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id++);
  ip->ip_p = _ip_p;
  ip->ip_src = _ip_src;
  ip->ip_dst = _ip_dst;

  if (p->ip_ttl_anno()) {
    ip->ip_tos = p->ip_tos_anno();
    /* We want to preserve the DF flag if set */
    ip->ip_off = htons(p->ip_off_anno() & IP_RF);
    ip->ip_ttl = p->ip_ttl_anno();
  } else {
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250; //rtm
  }

  ip->ip_sum = 0;
#ifndef __KERNEL__
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#else
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#endif
  
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip, sizeof(click_ip));
  
  return p;
}

EXPORT_ELEMENT(IPEncap)

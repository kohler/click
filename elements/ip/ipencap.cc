/*
 * ipencap.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris, Eddie Kohler, Alex Snoeren
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
#include "ipencap.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

IPEncap::IPEncap()
  : Element(1, 1), _ip_p(-1)
{
}

IPEncap::IPEncap(int ip_p, struct in_addr ip_src, struct in_addr ip_dst)
{
  add_input();
  add_output();
  _ip_p = ip_p;
  _ip_src = ip_src;
  _ip_dst = ip_dst;
}

IPEncap::~IPEncap()
{
}

IPEncap *
IPEncap::clone() const
{
  return new IPEncap(_ip_p, _ip_src, _ip_dst);
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
  if (_ip_p < 0)
    return errh->error("not configured");
  _id = 0;
  return 0;
}

Packet *
IPEncap::simple_action(Packet *p)
{
  /* What the fuck was going on here???  -  ED*/
  p = p->push(sizeof(click_ip));
  click_ip *ip = (click_ip *) p->data();
  memset(ip, '\0', sizeof(click_ip));
  
  ip->ip_v = IPVERSION;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id++);
  ip->ip_p = _ip_p;
  ip->ip_src = _ip_src;
  ip->ip_dst = _ip_dst;

  if(p->ip_ttl_anno()) {
    ip->ip_ttl = p->ip_ttl_anno();
    ip->ip_tos = p->ip_tos_anno();
    /* We want to preserve the DF flag if set */
    ip->ip_off = htons(p->ip_off_anno() & IP_RF);
  } else {
    click_chatter("IPEncap: Packet not annotated");
    ip->ip_ttl = 250; //rtm
  }

  ip->ip_sum = in_cksum(p->data(), sizeof(click_ip));
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip);
  
  return p;
}

EXPORT_ELEMENT(IPEncap)

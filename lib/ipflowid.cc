/*
 * ipflowid.{cc,hh} -- a TCP-UDP/IP connection class.
 * Eddie Kohler
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
#include "ipflowid.hh"
#include "click_ip.h"
#include "click_udp.h"
#include "packet.hh"
#include "confparse.hh"

IPFlowID::IPFlowID(Packet *p)
{
  const click_ip *iph = p->ip_header();
  _saddr = IPAddress(iph->ip_src.s_addr);
  _daddr = IPAddress(iph->ip_dst.s_addr);

  const click_udp *udph = (const click_udp *)p->transport_header();
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

IPFlowID2::IPFlowID2(Packet *p)
  : IPFlowID(p)
{
  const click_ip *iph = p->ip_header();
  _protocol = iph->ip_p;
}

String
IPFlowID::s() const
{
  const unsigned char *p = (const unsigned char *)&_saddr;
  const unsigned char *q = (const unsigned char *)&_daddr;
  String s;
  char tmp[128];
  sprintf(tmp, "(%d.%d.%d.%d, %hu, %d.%d.%d.%d, %hu)",
	  p[0], p[1], p[2], p[3], ntohs(_sport),
	  q[0], q[1], q[2], q[3], ntohs(_dport));
  return String(tmp);
}

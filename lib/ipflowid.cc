/*
 * ipflowid.{cc,hh} -- a TCP-UDP/IP connection class.
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/ipflowid.hh>
#include <click/click_ip.h>
#include <click/click_udp.h>
#include <click/packet.hh>
#include <click/confparse.hh>

IPFlowID::IPFlowID(Packet *p)
{
  const click_ip *iph = p->ip_header();
  const click_udp *udph = p->udp_header();
  assert(iph && udph);
  
  _saddr = IPAddress(iph->ip_src.s_addr);
  _daddr = IPAddress(iph->ip_dst.s_addr);
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

String
IPFlowID::unparse() const
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

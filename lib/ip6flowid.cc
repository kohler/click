/*
 * ip6flowid.{cc,hh} -- a TCP-UDP/IP connection class.
 * Eddie Kohler, Peilei Fan
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/ip6flowid.hh>
#include <click/click_ip6.h>
#include <click/click_udp.h>
#include <click/packet.hh>
#include <click/confparse.hh>

IP6FlowID::IP6FlowID(Packet *p)
{
  const click_ip6 *iph = p->ip6_header();
  _saddr = IP6Address(iph->ip6_src);
  _daddr = IP6Address(iph->ip6_dst);

  const click_udp *udph = (const click_udp *)p->transport_header();
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

String
IP6FlowID::s() const
{
  //  const unsigned char *p = (const unsigned char *)&_saddr;
  //  const unsigned char *q = (const unsigned char *)&_daddr; 
  //  String s;
  //  char tmp[128];
  //  sprintf(tmp, "(%d.%d.%d.%d, %hu, %d.%d.%d.%d, %hu)",
  //      p[0], p[1], p[2], p[3], ntohs(_sport),
  //  	  q[0], q[1], q[2], q[3], ntohs(_dport));
  //  return String(tmp);
  
  return _saddr.s() + ", " + String(ntohs(_sport)) + ", " +  _daddr.s()+ ", " + String(ntohs(_dport));
  
}

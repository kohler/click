/*
 * ipencap.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris, Eddie Kohler, Alex Snoeren
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

#include <click/config.h>
#include <click/package.hh>
#include "ipencap.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "elements/standard/alignmentinfo.hh"

IPEncap::IPEncap()
  : Element(1, 1), _ip_p(-1)
{
  MOD_INC_USE_COUNT;
}

IPEncap::~IPEncap()
{
  MOD_DEC_USE_COUNT;
}

IPEncap *
IPEncap::clone() const
{
  return new IPEncap;
}

int
IPEncap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned char ip_p_uc;
  if (cp_va_parse(conf, this, errh,
		  cpByte, "IP encapsulation protocol", &ip_p_uc,
		  cpIPAddress, "source IP address", &_ip_src,
		  cpIPAddress, "destination IP address", &_ip_dst,
		  0) < 0)
    return -1;
  _ip_p = ip_p_uc;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
  return 0;
}

int
IPEncap::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}

Packet *
IPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_ip));
  if (!p) return 0;
  
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.read_and_add(1));
  ip->ip_p = _ip_p;
  ip->ip_src = _ip_src;
  ip->ip_dst = _ip_dst;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned)
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  else
    ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif
  
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip, sizeof(click_ip));
  
  return p;
}

String
IPEncap::read_handler(Element *e, void *thunk)
{
  IPEncap *ipe = static_cast<IPEncap *>(e);
  switch ((int)thunk) {
   case 0:	return IPAddress(ipe->_ip_src).s() + "\n";
   case 1:	return IPAddress(ipe->_ip_dst).s() + "\n";
   default:	return "<error>\n";
  }
}

void
IPEncap::add_handlers()
{
  add_read_handler("src", read_handler, (void *)0);  
  add_write_handler("src", reconfigure_write_handler, (void *)1);
  add_read_handler("dst", read_handler, (void *)1);  
  add_write_handler("dst", reconfigure_write_handler, (void *)2);
}

EXPORT_ELEMENT(IPEncap)
ELEMENT_MT_SAFE(IPEncap)

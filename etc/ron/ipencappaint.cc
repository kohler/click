/*
 * ipencappaint.{cc,hh} -- element encapsulates packet in IP header, adds paint annotation
 * Alexander Yip
 * Based on IPEncap.
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include <click/config.h>
#endif
#include <click/config.h>
#include "ipencappaint.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>

IPEncapPaint::IPEncapPaint()
  : _ip_p(-1)
{
}

IPEncapPaint::~IPEncapPaint()
{
}

int
IPEncapPaint::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned char ip_p_uc;
  if (cp_va_parse(conf, this, errh,
		  cpByte, "color", &_color,
		  cpByte, "IP encapsulation protocol", &ip_p_uc,
		  cpIPAddress, "source IP address", &_ip_src,
		  cpEnd) < 0)
    return -1;
  _ip_p = ip_p_uc;

#ifdef __KERNEL__
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
IPEncapPaint::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}

Packet *
IPEncapPaint::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_ip) +1);
  click_ip *ip = reinterpret_cast<click_ip *>( p->data());

  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length() );
  ip->ip_id = htons(_id++);
  ip->ip_p = _ip_p;
  ip->ip_src = _ip_src;

  // set annotation in encapsulated packet
  p->data()[ sizeof(click_ip) ] = _color;

  // set dst to annotation dst
  ip->ip_dst = p_in->dst_ip_anno().in_addr();


  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
#ifdef __KERNEL__
  if (_aligned) {
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, (sizeof(click_ip)) >> 2);
  } else {
#endif
  ip->ip_sum = click_in_cksum((unsigned char *)ip, (sizeof(click_ip) ));
#ifdef __KERNEL__
  }
#endif

  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip, sizeof(click_ip));

  return p;
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
IPEncapPaint::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}


//ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(IPEncapPaint)

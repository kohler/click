/*
 * fixipsrc.{cc,hh} -- element sets IP source if Fix IP Source annotation on
 * Robert Morris
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
#include <click/config.h>
#include <click/package.hh>
#include "fixipsrc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/packet_anno.hh>

FixIPSrc::FixIPSrc()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

FixIPSrc::~FixIPSrc()
{
  MOD_DEC_USE_COUNT;
}

FixIPSrc *
FixIPSrc::clone() const
{
  return new FixIPSrc();
}

int
FixIPSrc::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  IPAddress a;

  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "local addr", &a,
		  0) < 0)
    return -1;
  _my_ip = a.in_addr();
  return 0;
}

WritablePacket *
FixIPSrc::fix_it(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  SET_FIX_IP_SRC_ANNO(p, 0);
#if 0
  click_chatter("FixIPSrc changed %x to %x",
                ip->ip_src.s_addr,
                _my_ip.s_addr);
#endif
  ip->ip_src = _my_ip;
  int hlen = ip->ip_hl << 2;
  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, hlen);
  return p;
}

Packet *
FixIPSrc::simple_action(Packet *p)
{
  const click_ip *ip = p->ip_header();
  if (FIX_IP_SRC_ANNO(p) && ip)
    p = fix_it(p);
  return p;
}

EXPORT_ELEMENT(FixIPSrc)
ELEMENT_MT_SAFE(FixIPSrc)


/*
 * getipaddress.{cc,hh} -- element sets IP destination annotation from
 * packet header
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

#include <click/config.h>
#include <click/package.hh>
#include "getipaddress.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ip.h>

GetIPAddress::GetIPAddress()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

GetIPAddress::~GetIPAddress()
{
  MOD_DEC_USE_COUNT;
}

int
GetIPAddress::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "byte offset of IP address", &_offset,
		     0);
}

Packet *
GetIPAddress::simple_action(Packet *p)
{
  p->set_dst_ip_anno(IPAddress(p->data() + _offset));
  return p;
}

EXPORT_ELEMENT(GetIPAddress)
ELEMENT_MT_SAFE(GetIPAddress)

/*
 * getipaddress.{cc,hh} -- element sets IP destination annotation from
 * packet header
 * Robert Morris
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
#include "getipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ip.h"

GetIPAddress::GetIPAddress()
  : Element(1, 1)
{
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

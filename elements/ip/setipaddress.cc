/*
 * setipaddress.{cc,hh} -- element sets IP destination to value of
 * destination address annotation
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
#include "setipaddress.hh"
#include "confparse.hh"
#include <string.h>

SetIPAddress::SetIPAddress()
  : Element(1, 1)
{
}

int
SetIPAddress::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "byte offset of IP address", &_offset,
		     0);
}

Packet *
SetIPAddress::simple_action(Packet *p)
{
  IPAddress ipa = p->dst_ip_anno();
  if (ipa && _offset + 4 <= p->length())
    memcpy(p->data() + _offset, &ipa, 4);
  // XXX error reporting?
  return p;
}

EXPORT_ELEMENT(SetIPAddress)

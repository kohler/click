/*
 * setipaddress.{cc,hh} -- element sets destination address annotation
 * to a particular IP address
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
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
		     cpIPAddress, "IP address", &_ip,
		     0);
}

Packet *
SetIPAddress::simple_action(Packet *p)
{
  p->set_dst_ip_anno(_ip);
  return p;
}

EXPORT_ELEMENT(SetIPAddress)

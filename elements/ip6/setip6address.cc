/*
 * setip6address.{cc,hh} -- element sets destination address annotation
 * to a particular IP6 address
 * Eddie Kohler, Peilei Fan
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
#include "setip6address.hh"
#include "confparse.hh"

SetIP6Address::SetIP6Address()
  : Element(1, 1)
{
}

int
SetIP6Address::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpIP6Address, "IP6 address", &_ip6,
		     0);
}

Packet *
SetIP6Address::simple_action(Packet *p)
{
  p->set_dst_ip6_anno(_ip6);
  return p;
}

EXPORT_ELEMENT(SetIP6Address)

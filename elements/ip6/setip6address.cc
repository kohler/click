/*
 * setip6address.{cc,hh} -- element sets destination address annotation
 * to a particular IP6 address
 * Eddie Kohler, Peilei Fan
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "setip6address.hh"
#include <click/confparse.hh>

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

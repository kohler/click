/*
 * setipaddress.{cc,hh} -- element sets destination address annotation
 * to a particular IP address
 * Eddie Kohler
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
#include <click/config.h>
#include <click/package.hh>
#include "setipaddress.hh"
#include <click/confparse.hh>

SetIPAddress::SetIPAddress()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetIPAddress::~SetIPAddress()
{
  MOD_DEC_USE_COUNT;
}

int
SetIPAddress::configure(const Vector<String> &conf, ErrorHandler *errh)
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
ELEMENT_MT_SAFE(SetIPAddress)


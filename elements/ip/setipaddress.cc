/*
 * setipaddress.{cc,hh} -- element sets destination address annotation
 * to a particular IP address
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
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
SetIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
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

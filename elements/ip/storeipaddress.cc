/*
 * storeipaddress.{cc,hh} -- element stores IP destination annotation into
 * packet
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
#include "storeipaddress.hh"
#include <click/confparse.hh>

StoreIPAddress::StoreIPAddress()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

StoreIPAddress::~StoreIPAddress()
{
  MOD_DEC_USE_COUNT;
}

int
StoreIPAddress::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "byte offset of IP address", &_offset,
		     0);
}

Packet *
StoreIPAddress::simple_action(Packet *p)
{
  // XXX error reporting?
  IPAddress ipa = p->dst_ip_anno();
  if (ipa && _offset + 4 <= p->length()) {
    WritablePacket *q = p->uniqueify();
    memcpy(q->data() + _offset, &ipa, 4);
    return q;
  } else
    return p;
}

EXPORT_ELEMENT(StoreIPAddress)
ELEMENT_MT_SAFE(StoreIPAddress)

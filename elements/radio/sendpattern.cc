/*
 * sendpattern.{cc,hh} -- element creates a particular kind of packet for
 * CheckPattern to check
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
#include "sendpattern.hh"
#include <click/confparse.hh>
#include <click/error.hh>

SendPattern::SendPattern()
  : Element(0, 1)
{
  MOD_INC_USE_COUNT;
  _len = 1;
}

SendPattern::~SendPattern()
{
  MOD_DEC_USE_COUNT;
}

SendPattern *
SendPattern::clone() const
{
  return new SendPattern();
}

int
SendPattern::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "packet length", &_len,
		     0);
}

Packet *
SendPattern::pull(int)
{
  WritablePacket *p = Packet::make(_len);
  int i;
  for(i = 0; i < _len; i++)
    p->data()[i] = i & 0xff;
  return p;
}

EXPORT_ELEMENT(SendPattern)

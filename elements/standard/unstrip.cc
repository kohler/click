/*
 * unstrip.{cc,hh} -- element unstrips bytes from front of packet
 * Robert Morris, Eddie Kohler
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
#include "unstrip.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

Unstrip::Unstrip(unsigned nbytes)
  : Element(1, 1), _nbytes(nbytes)
{
  MOD_INC_USE_COUNT;
}

Unstrip::~Unstrip()
{
  MOD_DEC_USE_COUNT;
}

Unstrip *
Unstrip::clone() const
{
  return new Unstrip(_nbytes);
}

int
Unstrip::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "number of bytes to unstrip", &_nbytes,
		     0);
}

Packet *
Unstrip::simple_action(Packet *p)
{
  return p->push(_nbytes);
}

EXPORT_ELEMENT(Unstrip)
ELEMENT_MT_SAFE(Unstrip)

/*
 * strip.{cc,hh} -- element strips bytes from front of packet
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "strip.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

Strip::Strip(unsigned nbytes)
  : Element(1, 1), _nbytes(nbytes)
{
  MOD_INC_USE_COUNT;
}

Strip::~Strip()
{
  MOD_DEC_USE_COUNT;
}

Strip *
Strip::clone() const
{
  return new Strip(_nbytes);
}

int
Strip::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "number of bytes to strip", &_nbytes,
		     0);
}

Packet *
Strip::simple_action(Packet *p)
{
  p->pull(_nbytes);
  return p;
}

EXPORT_ELEMENT(Strip)
ELEMENT_MT_SAFE(Strip)

/*
 * strip.{cc,hh} -- element strips bytes from front of packet
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "strip.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

Strip::Strip(unsigned nbytes)
  : Element(1, 1), _nbytes(nbytes)
{
}

Strip::~Strip()
{
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

/*
 * strip.{cc,hh} -- element strips bytes from front of packet
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "strip.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

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
Strip::configure(const String &conf, ErrorHandler *errh)
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

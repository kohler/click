/*
 * sendpattern.{cc,hh} -- element creates a particular kind of packet for
 * CheckPattern to check
 * Robert Morris
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
#include "sendpattern.hh"
#include "confparse.hh"
#include "error.hh"

SendPattern::SendPattern()
{
  add_output();
  _len = 1;
}

SendPattern *
SendPattern::clone() const
{
  return new SendPattern();
}

int
SendPattern::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "packet length", &_len,
		     0);
}

Packet *
SendPattern::pull(int)
{
  Packet *p = Packet::make(_len);
  int i;
  for(i = 0; i < _len; i++)
    p->data()[i] = i & 0xff;
  return(p);
}

EXPORT_ELEMENT(SendPattern)

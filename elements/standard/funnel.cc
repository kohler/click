/*
 * funnel.{cc,hh} -- element funnels packets
 * Thomer M. Gil
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
#include "funnel.hh"
#include "confparse.hh"
#include "error.hh"

Funnel *
Funnel::clone() const
{
  return new Funnel;
}

int
Funnel::configure(const String &conf, ErrorHandler *errh)
{
  int n = ninputs();
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "number of arms", &n, 0) < 0)
    return -1;
  set_ninputs(n);
  return 0;
}

void
Funnel::push(int, Packet *p)
{
  output(0).push(p);
}

EXPORT_ELEMENT(Funnel)

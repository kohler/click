/*
 * priosched.{cc,hh} -- priority scheduler element
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
#include "priosched.hh"

PrioSched::PrioSched()
{
  add_output();
}

PrioSched::~PrioSched()
{
}

void
PrioSched::notify_ninputs(int i)
{
  set_ninputs(i);
}

Packet *
PrioSched::pull(int)
{
  for (int i = 0; i < ninputs(); i++) {
    Packet *p = input(i).pull();
    if (p)
      return p;
  }
  return 0;
}

EXPORT_ELEMENT(PrioSched)

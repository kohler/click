/*
 * rrsched.{cc,hh} -- round robin scheduler element
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
#include "rrsched.hh"

RRSched::RRSched()
{
  add_output();
  _next = 0;
}

RRSched::~RRSched()
{
}

void
RRSched::notify_ninputs(int i)
{
  set_ninputs(i);
}

Packet *
RRSched::pull(int)
{
  int n = ninputs();
  int i = _next;
  for (int j = 0; j < n; j++) {
    Packet *p = input(i).pull();
    i++;
    if (i >= n) i = 0;
    if (p) {
      _next = i;
      return p;
    }
  }
  return 0;
}

EXPORT_ELEMENT(RRSched)

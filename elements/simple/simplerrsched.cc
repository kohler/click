/*
 * rrsched.{cc,hh} -- round robin scheduler element
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
#include "simplerrsched.hh"
CLICK_DECLS

SimpleRRSched::SimpleRRSched()
{
  _next = 0;
}

SimpleRRSched::~SimpleRRSched()
{
}

Packet *
SimpleRRSched::pull(int)
{
  int n = ninputs();
  int i = _next;
  for (int j = 0; j < n; j++) {
    Packet *p = input(i).pull();
    i++;
    if (i >= n)
      i = 0;
    if (p) {
      _next = i;
      return p;
    }
  }
  return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimpleRRSched)

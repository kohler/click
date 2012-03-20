/*
 * rrswitch.{cc,hh} -- round robin switch element
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "rrswitch.hh"
CLICK_DECLS

RoundRobinSwitch::RoundRobinSwitch()
{
  _next = 0;
}

void
RoundRobinSwitch::push(int, Packet *p)
{
  int i = _next;
#ifndef __MTCLICK__
  _next++;
  if (_next >= (uint32_t)noutputs())
    _next = 0;
#else
  // in MT case try our best to be rr, but don't worry about it if we mess up
  // once in awhile
  int newval = i+1;
  if (newval >= noutputs())
    newval = 0;
  _next.compare_swap(i, newval);
#endif
  output(i).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RoundRobinSwitch)
ELEMENT_MT_SAFE(RoundRobinSwitch)

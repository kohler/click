/*
 * rrswitch.{cc,hh} -- round robin switch element
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "rrswitch.hh"

RoundRobinSwitch::RoundRobinSwitch()
{
  MOD_INC_USE_COUNT;
  add_input();
  _next = 0;
}

RoundRobinSwitch::~RoundRobinSwitch()
{
  MOD_DEC_USE_COUNT;
}

void
RoundRobinSwitch::notify_noutputs(int i)
{
  set_noutputs(i < 1 ? 0 : i);
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
  _next.compare_and_swap(i, newval);
#endif
  output(i).push(p);
}

EXPORT_ELEMENT(RoundRobinSwitch)
ELEMENT_MT_SAFE(RoundRobinSwitch)


/*
 * rtcycles.{cc,hh} -- measures round trip cycles on a push or pull path.
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
#include "rtcycles.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

RTCycles::RTCycles()
{
  add_input();
  add_output();
  _accum = _npackets = 0;
}

void
RTCycles::push(int, Packet *p)
{
  unsigned long long c = click_get_cycles();
  output(0).push(p);
  _accum += click_get_cycles() - c;
  _npackets++;
}

Packet *
RTCycles::pull(int)
{
  unsigned long long c = click_get_cycles();
  Packet *p = input(0).pull();
  _accum += click_get_cycles() - c;
  if (p) _npackets++;
  return(p);
}

static String
RTCycles_read_cycles(Element *e, void *thunk)
{
  RTCycles *s = static_cast<RTCycles *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(s->_npackets) + "\n";
   case 1:
    return String(s->_accum) + "\n";
   default:
    return String();
  }
}

static int
RTCycles_reset_counts(const String &, Element *e, void *, ErrorHandler *)
{
  RTCycles *s = static_cast<RTCycles *>(e);
  s->_npackets = 0;
  s->_accum = 0;
  return 0;
}

void
RTCycles::add_handlers()
{
  add_read_handler("packets", RTCycles_read_cycles, (void *)0);
  add_read_handler("cycles", RTCycles_read_cycles, (void *)1);
  add_write_handler("reset_counts", RTCycles_reset_counts, 0);
}


ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(RTCycles)

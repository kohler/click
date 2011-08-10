/*
 * rtcycles.{cc,hh} -- measures round trip cycles on a push or pull path.
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
#include "rtcycles.hh"
#include <click/error.hh>
#include <click/glue.hh>

RTCycles::RTCycles()
{
  _accum = _npackets = 0;
}

RTCycles::~RTCycles()
{
}

void
RTCycles::push(int, Packet *p)
{
  click_cycles_t c = click_get_cycles();
  output(0).push(p);
  _accum += click_get_cycles() - c;
  _npackets++;
}

Packet *
RTCycles::pull(int)
{
  click_cycles_t c = click_get_cycles();
  Packet *p = input(0).pull();
  _accum += click_get_cycles() - c;
  if (p) _npackets++;
  return(p);
}

static String
RTCycles_read_cycles(Element *e, void *thunk)
{
  RTCycles *s = static_cast<RTCycles *>(e);
  switch (reinterpret_cast<intptr_t>(thunk)) {
   case 0:
    return String(s->_npackets);
   case 1:
    return String(s->_accum);
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
  add_read_handler("packets", RTCycles_read_cycles, 0);
  add_read_handler("cycles", RTCycles_read_cycles, 1);
  add_write_handler("reset_counts", RTCycles_reset_counts, 0, Handler::BUTTON);
}


ELEMENT_REQUIRES(linuxmodule int64)
EXPORT_ELEMENT(RTCycles)

/*
 * rtcycles.{cc,hh} -- measures round trip cycles on a push or pull path.
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
#include "rtcycles.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

RTCycles::RTCycles()
  : _sum(0), _pkts(0)
{
  add_input();
  add_output();
}

RTCycles::~RTCycles()
{
}

void
RTCycles::push(int, Packet *p)
{
  unsigned long long c = click_get_cycles();
  output(0).push(p);
  _sum += click_get_cycles() - c;
  if (p) _pkts++;
}

Packet *
RTCycles::pull(int)
{
  unsigned long long c = click_get_cycles();
  Packet *p = input(0).pull();
  _sum += click_get_cycles() - c;
  if (p) _pkts++;
  return(p);
}

static String
RTCycles_read_cycles(Element *f, void *)
{
  RTCycles *s = (RTCycles *)f;
  return
    String(s->_sum) + " cycles\n" +
    String(s->_pkts) + " packets\n";
}

void
RTCycles::add_handlers()
{
  add_read_handler("cycles", RTCycles_read_cycles, 0);
}


ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(RTCycles)

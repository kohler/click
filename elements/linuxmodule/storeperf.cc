/*
 * storeperf.{cc,hh} -- store icache/dcache info
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
#include "storeperf.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

StorePerf::StorePerf()
{
  add_input();
  add_output();
  _m0 = _m1 = _packets = 0;
}

StorePerf::~StorePerf()
{
}

StorePerf *
StorePerf::clone() const
{
  return new StorePerf();
}

inline void
StorePerf::smaction(Packet *p)
{
  unsigned m00 = p->metric0_anno(0), m01 = p->metric0_anno(1);
  unsigned m10 = p->metric1_anno(0), m11 = p->metric1_anno(1);

  _m0 += (m01 >= m00) ? m01 - m00 : (UINT_MAX - m00 + m01);
  _m1 += (m11 >= m10) ? m11 - m10 : (UINT_MAX - m10 + m11);
  _packets++;
}

void
StorePerf::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
StorePerf::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

static String
StorePerf_read_cycles(Element *f, void *)
{
  StorePerf *s = (StorePerf *)f;
  return
    String(s->_m0) + " (metric1)\n" +
    String(s->_m1) + " (metric2)\n" +
    String(s->_packets) + " packets\n";
}

void
StorePerf::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("stats", StorePerf_read_cycles, 0);
}

EXPORT_ELEMENT(StorePerf)

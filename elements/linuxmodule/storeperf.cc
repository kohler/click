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
  _icache = _dcache = _packets = 0;
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
  unsigned i0 = p->icache_anno(0), i1 = p->icache_anno(1);
  unsigned d0 = p->dcache_anno(0), d1 = p->dcache_anno(1);

  _icache += (i1 >= i0) ? i1 - i0 : (UINT_MAX - i0 + i1);
  _dcache += (d1 >= d0) ? d1 - d0 : (UINT_MAX - d0 + d1);
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
    String(s->_dcache) + " dcache misses\n" +
    String(s->_icache) + " icache misses\n" +
    String(s->_packets) + " packets\n";
}

void
StorePerf::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("stats", StorePerf_read_cycles, 0);
}

EXPORT_ELEMENT(StorePerf)

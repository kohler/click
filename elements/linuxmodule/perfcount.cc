/*
 * perfcount.{cc,hh} -- read performance counters
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
#include "perfcount.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "asm/msr.h"

PerfCount::PerfCount()
{
  add_input();
  add_output();
  _m0 = _m1 = _packets = 0;
}

PerfCount::~PerfCount()
{
}

PerfCount *
PerfCount::clone() const
{
  return new PerfCount();
}

int
PerfCount::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "index", &_idx,
		  cpEnd) < 0)
    return -1;

  if (_idx > 1)
    return errh->error("index must be 0 or 1");

  return 0;
}

inline void
PerfCount::smaction(Packet *p)
{
  unsigned low, high;

  rdpmc(0, low, high);
  p->set_metric0_anno(_idx, low);

  rdpmc(1, low, high);
  p->set_metric1_anno(_idx, low);

  if (_idx == 1) {
    unsigned m00 = p->metric0_anno(0), m01 = p->metric0_anno(1);
    unsigned m10 = p->metric1_anno(0), m11 = p->metric1_anno(1);
    
    _m0 += (m01 >= m00) ? m01 - m00 : (UINT_MAX - m00 + m01);
    _m1 += (m11 >= m10) ? m11 - m10 : (UINT_MAX - m10 + m11);
    _packets++;
  }
}

void
PerfCount::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
PerfCount::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

static String
PerfCount_read_cycles(Element *f, void *)
{
  PerfCount *s = (PerfCount *)f;
  return
    String(s->_m0) + " (metric1)\n" +
    String(s->_m1) + " (metric2)\n" +
    String(s->_packets) + " packets\n";
}

void
PerfCount::add_handlers()
{
  add_read_handler("stats", PerfCount_read_cycles, 0);
}

ELEMENT_REQUIRES(linuxmodule false)
EXPORT_ELEMENT(PerfCount)

/*
 * perfcount.{cc,hh} -- read performance counters
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
#include "perfcount.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "asm/msr.h"


PerfCount::PerfCount()
{
  add_input();
  add_output();
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
PerfCount::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "index", &_idx,
		     0);
}

int
PerfCount::initialize(ErrorHandler *errh)
{
  if (_idx > 1)
    return errh->error("index must be 0 or 1");

  // Count icache misses on counter 0, dcache misses on counter 1
  wrmsr(MSR_EVNTSEL0, IFU_IFETCH_MISS|MSR_FLAGS0, 0);
  wrmsr(MSR_EVNTSEL1, DCU_MISS_OUTSTANDING|MSR_FLAGS1, 0);
  return 0;
}

inline void
PerfCount::smaction(Packet *p)
{
  unsigned low, high;

  rdpmc(0, low, high);
  p->set_icache_anno(_idx, low);

  rdpmc(1, low, high);
  p->set_dcache_anno(_idx, low);
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

EXPORT_ELEMENT(PerfCount)

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

static unsigned PerfCount::_init = 0;
static HashMap<String, unsigned> PerfCount::_metrics;

PerfCount::PerfCount()
{
  add_input();
  add_output();
  if (!_init) {
    _metrics.insert("DCU_MISS_OUTSTANDING", 0x48);
    _metrics.insert("IFU_IFETCH_MISS", 0x81);
    _metrics.insert("L2_IFETCH", 0x28 | (0xf<<8));
    _metrics.insert("L2_LD", 0x29 | (0xf<<8));
    _metrics.insert("L2_RQSTS", 0x2e | (0xf<<8));
    _init = 1;
  }
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
  String metric0, metric1;

  _metric0 = _metrics["DCU_MISS_OUTSTANDING"];
  _metric1 = _metrics["IFU_IFETCH_MISS"];

  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "index", &_idx,
		  cpOptional,
		  cpString, "first metric", &metric0,
		  cpString, "second metric", &metric1,
		  cpEnd) < 0)
    return -1;

  if (!(_metric0 = _metrics[metric0])) {
    errh->error("unknown first metric");
    return -1;
  }

  if (!(_metric1 = _metrics[metric1])) {
    errh->error("unknown second metric");
    return -1;
  }
  return 0;
}

int
PerfCount::initialize(ErrorHandler *errh)
{
  if (_idx > 1)
    return errh->error("index must be 0 or 1");

  // Count metric0 on counter 0, metric1 on counter 1
  wrmsr(MSR_EVNTSEL0, _metric0|MSR_FLAGS0, 0);
  wrmsr(MSR_EVNTSEL1, _metric1|MSR_FLAGS1, 0);
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

#include "hashmap.cc"
template class HashMap<String, unsigned>;

EXPORT_ELEMENT(PerfCount)


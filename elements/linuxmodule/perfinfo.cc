/*
 * perfinfo.{cc,hh} -- set up performance counters
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
#include "perfinfo.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "asm/msr.h"

static unsigned PerfInfo::_init = 0;
static unsigned PerfInfo::_metric0 = 0;
static unsigned PerfInfo::_metric1 = 0;
static HashMap<String, unsigned> PerfInfo::_metrics;

PerfInfo *
PerfInfo::clone() const
{
  return new PerfInfo();
}

int
PerfInfo::configure(const String &conf, ErrorHandler *errh)
{
  String metric0, metric1;
  
  if (_init)
    return errh->error("only one PerfInfo element allowed per configuration"); 

  _metrics.insert("DCU_MISS_OUTSTANDING", 0x48);
  _metrics.insert("IFU_IFETCH_MISS", 0x81);
  _metrics.insert("L2_IFETCH", 0x28 | (0xf<<8));
  _metrics.insert("L2_LD", 0x29 | (0xf<<8));
  _metrics.insert("L2_RQSTS", 0x2e | (0xf<<8));
    
  _metric0 = _metrics["DCU_MISS_OUTSTANDING"];
  _metric1 = _metrics["IFU_IFETCH_MISS"];
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "first metric", &metric0,
		  cpString, "second metric", &metric1,
		  cpEnd) < 0)
    return -1;

  if (!(_metric0 = _metrics[metric0]))
    return errh->error("unknown first metric");

  if (!(_metric1 = _metrics[metric1]))
    return errh->error("unknown second metric");

  _init = 1;
  return 0;
}

int
PerfInfo::initialize(ErrorHandler *errh)
{
  wrmsr(MSR_EVNTSEL0, _metric0|MSR_FLAGS0, 0);
  wrmsr(MSR_EVNTSEL1, _metric1|MSR_FLAGS1, 0);
  return 0;
}

void
PerfInfo::uninitialize()
{
  _metrics.clear();
  _init = 0;
}


#include "hashmap.cc"
template class HashMap<String, unsigned>;

EXPORT_ELEMENT(PerfInfo)


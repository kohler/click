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

unsigned PerfInfo::_init = 0;
unsigned PerfInfo::_metric0 = 0;
unsigned PerfInfo::_metric1 = 0;

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

  HashMap<String, int> metrics(0);
  metrics.insert("DCU_MISS_OUTSTANDING", 0x48);
  metrics.insert("INST_RETIRED", 0xC0);
  metrics.insert("IFU_FETCH", 0x80);
  metrics.insert("IFU_FETCH_MISS", 0x81);
  metrics.insert("L2_IFETCH", 0x28 | (0xf<<8));
  metrics.insert("L2_LD", 0x29 | (0xf<<8));
  metrics.insert("L2_LINES_OUTM", 0x27);
  metrics.insert("L2_RQSTS", 0x2e | (0xf<<8));
  metrics.insert("BUS_TRAN_MEM", 0x6f);
  metrics.insert("BUS_TRAN_INVAL", 0x69);
    
  _metric0 = metrics["DCU_MISS_OUTSTANDING"];
  _metric1 = metrics["IFU_IFETCH_MISS"];
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "first metric", &metric0,
		  cpString, "second metric", &metric1,
		  cpEnd) < 0)
    return -1;

  if (!(_metric0 = metrics[metric0]))
    return errh->error("unknown first metric");

  if (!(_metric1 = metrics[metric1]))
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
  _init = 0;
}

EXPORT_ELEMENT(PerfInfo)


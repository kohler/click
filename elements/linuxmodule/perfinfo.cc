/*
 * perfinfo.{cc,hh} -- set up performance counters
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
#include "perfctr.hh"
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
PerfInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String metric0, metric1;
  
  if (_init)
    return errh->error("only one PerfInfo element allowed per configuration"); 

  HashMap<String, int> metrics(0);
  metrics.insert("DCU_MISS_OUTSTANDING", DCU_MISS_OUTSTANDING);
  metrics.insert("INST_RETIRED", INST_RETIRED);
  metrics.insert("IFU_FETCH", IFU_FETCH);
  metrics.insert("IFU_FETCH_MISS", IFU_FETCH_MISS);
  metrics.insert("IFU_MEM_STALL", IFU_MEM_STALL);
  metrics.insert("L2_IFETCH", L2_IFETCH);
  metrics.insert("L2_LD", L2_LD);
  metrics.insert("L2_LINES_OUTM", L2_LINES_OUTM);
  metrics.insert("L2_RQSTS", L2_RQSTS);
  metrics.insert("BUS_TRAN_MEM", BUS_TRAN_MEM);
  metrics.insert("BUS_TRAN_INVAL", BUS_TRAN_INVAL);
  metrics.insert("L2_LINES_IN", L2_LINES_IN);
  metrics.insert("L2_LINES_OUT", L2_LINES_OUT);
    
  _metric0 = metrics["DCU_MISS_OUTSTANDING"];
  _metric1 = metrics["IFU_IFETCH_MISS"];
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpWord, "first metric", &metric0,
		  cpWord, "second metric", &metric1,
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

ELEMENT_REQUIRES(linuxmodule false)
EXPORT_ELEMENT(PerfInfo)

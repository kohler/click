/*
 * perfcountuser.{cc,hh} -- performance counter helpers
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "perfcountuser.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/glue.hh>
#include <asm/msr.h>
#include <click/perfctr.hh>

PerfCountUser::PerfCountUser()
{
  _metric0 = _metric1 = -2;
}

static int
string_to_perfctr(const String &name_in)
{
  String name = name_in.upper();
#define TRY(x)		if (name == #x) return x;
  TRY(BUS_TRAN_INVAL);
  TRY(BUS_TRAN_MEM);
  TRY(DCU_MISS_OUTSTANDING);
  TRY(IFU_FETCH);
  TRY(IFU_FETCH_MISS);
  TRY(IFU_MEM_STALL);
  TRY(INST_RETIRED);
  TRY(L2_IFETCH);
  TRY(L2_LD);
  TRY(L2_LINES_IN);
  TRY(L2_LINES_OUT);
  TRY(L2_LINES_OUTM);
  TRY(L2_RQSTS);
  return -1;
#undef TRY
}

int
PerfCountUser::prepare(const String &name, ErrorHandler *errh)
{
  int which = string_to_perfctr(name);
  if (which == -1)
    return errh->error("unknown performance metric `%s'", String(name).cc());
  
  // find the base PerfCountUser
  PerfCountUser *base = 0;
  for (int ei = 0; ei < router()->nelements() && !base; ei++) {
    Element *e = router()->element(ei);
    if (e == this) continue;
    if (PerfCountUser *pc = (PerfCountUser *)e->cast("PerfCountUser")) {
      if (pc->is_base())
	base = pc;
    }
  }
  if (!base) {
    _metric0 = -1;
    base = this;
  }

  if (_metric0 < 0) {
    _metric0 = which;
    return 0;
  } else if (_metric0 == which)
    return 0;
  else if (_metric1 < 0) {
    _metric1 = which;
    return 1;
  } else if (_metric1 == which)
    return 1;
  else
    return errh->error("configuration uses too many performance metrics\n(I can only keep track of two different metrics, maximum.)");
}

int
PerfCountUser::initialize(ErrorHandler *)
{
  if (_metric0 >= 0)
    wrmsr(MSR_EVNTSEL0, _metric0 | MSR_FLAGS0, 0);
  if (_metric1 >= 0)
    wrmsr(MSR_EVNTSEL1, _metric1 | MSR_FLAGS1, 0);
  return 0;
}

ELEMENT_REQUIRES(linuxmodule)
ELEMENT_PROVIDES(PerfCountUser)

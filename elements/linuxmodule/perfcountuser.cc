/*
 * perfcountuser.{cc,hh} -- performance counter helpers
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "perfcountuser.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/glue.hh>
#include <click/perfctr-i586.hh>

PerfCountUser::PerfCountUser()
{
  _metric0 = _metric1 = -2;
}

PerfCountUser::~PerfCountUser()
{
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
  TRY(BUS_LOCK_CLOCKS);
#undef TRY
  return -1;
}

int
PerfCountUser::prepare(const String &name, ErrorHandler *errh, int force)
{
  int which = string_to_perfctr(name);
  if (which == -1)
    return errh->error("unknown performance metric '%s'", name.c_str());

  // find the base PerfCountUser
  PerfCountUser *base = 0;
  for (int ei = 0; ei < router()->nelements() && !base; ei++) {
    Element *e = router()->element(ei);
    if (PerfCountUser *pc = (PerfCountUser *)e->cast("PerfCountUser")) {
      if (pc->is_base())
	base = pc;
    }
  }
  if (!base) {
    _metric0 = _metric1 = -1;
    base = this;
  }

  if ((force < 0 || force == 0)
      && (base->_metric0 < 0 || base->_metric0 == which)) {
    base->_metric0 = which;
    return 0;
  } else if ((force < 0 || force == 1)
	     && (base->_metric1 < 0 || base->_metric1 == which)) {
    base->_metric1 = which;
    return 1;
  } else if (force < 0)
    return errh->error("configuration uses too many performance metrics\n(I can keep track of at most two different metrics.)");
  else
    return errh->error("performance metric %d already used", force);
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

ELEMENT_REQUIRES(linuxmodule i586)
ELEMENT_PROVIDES(PerfCountUser)

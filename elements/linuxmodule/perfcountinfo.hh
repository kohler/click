#ifndef PERFCOUNTINFO_HH
#define PERFCOUNTINFO_HH
#include "elements/linuxmodule/perfcountuser.hh"

/*
 * =c
 * PerfCountInfo([METRIC0, METRIC1])
 *
 * =s counters
 * turn on Pentium Pro performance metrics
 *
 * =d
 *
 * The Pentium Pro CPU can maintain up to two performance counters.
 * PerfCountInfo turns on those counters. METRIC0 is the type of performance
 * counter 0, METRIC1 is the type of performance counter 1. Either type may be
 * empty, in which case that counter will not be used.
 *
 * The SetPerfCount and PerfCountAccum elements automatically turn on the
 * relevant counters, so PerfCountInfo is not normally needed. It is useful if
 * you are instrumenting individual elements.
 *
 * =n
 *
 * Valid performance metric names are C<BUS_TRAN_INVAL>, C<BUS_TRAN_MEM>,
 * C<DCU_MISS_OUTSTANDING>, C<IFU_FETCH>, C<IFU_FETCH_MISS>, C<IFU_MEM_STALL>,
 * C<INST_RETIRED>, C<L2_IFETCH>, C<L2_LD>, C<L2_LINES_IN>, C<L2_LINES_OUT>,
 * C<L2_LINES_OUTM>, and C<L2_RQSTS>.
 *
 * =a
 * SetPerfCount, PerfCountAccum, SetCycleCount, CycleCountAccum */

class PerfCountInfo : public PerfCountUser { public:

  PerfCountInfo() CLICK_COLD;
  ~PerfCountInfo() CLICK_COLD;

  const char *class_name() const	{ return "PerfCountInfo"; }
  void *cast(const char *);
  int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

};

#endif

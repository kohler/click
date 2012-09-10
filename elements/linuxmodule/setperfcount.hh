#ifndef SETPERFCOUNT_HH
#define SETPERFCOUNT_HH
#include "elements/linuxmodule/perfcountuser.hh"

/*
 * =c
 * SetPerfCount(METRIC)
 * =s counters
 * stores Pentium Pro performance metric in annotation
 * =d
 *
 * Stores information about the Pentium Pro performance metric METRIC in each
 * packet. In combination with PerfCountAccum, this lets you measure how the
 * metric changes over the packet's lifetime.
 *
 * =n
 *
 * A packet has room for either exactly one cycle count or exactly one
 * performance metric.
 *
 * Valid performance metric names are C<BUS_TRAN_INVAL>, C<BUS_TRAN_MEM>,
 * C<DCU_MISS_OUTSTANDING>, C<IFU_FETCH>, C<IFU_FETCH_MISS>, C<IFU_MEM_STALL>,
 * C<INST_RETIRED>, C<L2_IFETCH>, C<L2_LD>, C<L2_LINES_IN>, C<L2_LINES_OUT>,
 * C<L2_LINES_OUTM>, and C<L2_RQSTS>.
 *
 * =a PerfCountAccum, SetCycleCount, CycleCountAccum */

class SetPerfCount : public PerfCountUser { public:

  SetPerfCount() CLICK_COLD;
  ~SetPerfCount() CLICK_COLD;

  const char *class_name() const		{ return "SetPerfCount"; }
  void *cast(const char *);
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:

  int _which;

};

#endif

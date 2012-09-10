#ifndef PERFCOUNTACCUM_HH
#define PERFCOUNTACCUM_HH

/*
 * =c
 * PerfCountAccum(METRIC)
 * =s counters
 * collects differences in Pentium Pro performance metrics
 * =d
 *
 * Expects incoming packets to have their performance metric annotation set
 * according to the Pentium Pro performance metric METRIC. Calculates the
 * current value of the performance metric METRIC, and keeps track of the total
 * accumulated difference.
 *
 * =n
 *
 * A packet has room for either exactly one cycle count or exactly one
 * performance metric.
 *
 * See SetPerfCount for valid performance metric names.
 *
 * =h count read-only
 * Returns the number of packets that have passed.
 *
 * =h accum read-only
 * Returns the accumulated changes in the metric TYPE for all passing packets.
 *
 * =h reset_counts write-only
 * Resets C<count> and C<accum> counters to zero when written.
 *
 * =a SetPerfCount, SetCycleCount, CycleCountAccum */

#include "elements/linuxmodule/perfcountuser.hh"

class PerfCountAccum : public PerfCountUser { public:

  PerfCountAccum() CLICK_COLD;
  ~PerfCountAccum() CLICK_COLD;

  const char *class_name() const		{ return "PerfCountAccum"; }
  void *cast(const char *);
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:

  int _which;
  uint64_t _accum;
  uint64_t _count;

  static String read_handler(Element *, void *) CLICK_COLD;
  static int reset_handler(const String &, Element *, void *, ErrorHandler *);

};

#endif

#ifndef PERFCOUNTACCUM_HH
#define PERFCOUNTACCUM_HH

/*
 * =c
 * PerfCountAccum(TYPE)
 * =s profiling
 * collects differences in Pentium Pro performance metrics
 * =d
 *
 * Expects incoming packets to have their performance metric annotation set
 * according to the Pentium Pro performance metric TYPE. Calculates the
 * current value of the performance metric TYPE, and keeps track of the total
 * accumulated difference.
 *
 * =n
 *
 * A packet has room for either exactly one cycle count or exactly one
 * performance metric.
 *
 * See SetPerfCount for valid performance metric names.
 *
 * =h packets read-only
 * Returns the number of packets that have passed.
 *
 * =h accum read-only
 * Returns the accumulated changes in the metric TYPE for all passing packets.
 *
 * =h reset_counts write-only
 * Resets C<packets> and C<accum> counters to zero when written.
 * 
 * =a SetPerfCount, SetCycleCount, CycleCountAccum */

#include "elements/linuxmodule/perfcountuser.hh"

class PerfCountAccum : public PerfCountUser { public:
  
  PerfCountAccum();
  ~PerfCountAccum();
  
  const char *class_name() const		{ return "PerfCountAccum"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  PerfCountAccum *clone() const;

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:
  
  int _which;
  unsigned long long _accum;
  unsigned long long _npackets;

  static String read_handler(Element *, void *);
  static int reset_handler(const String &, Element *, void *, ErrorHandler *);
  
};

#endif

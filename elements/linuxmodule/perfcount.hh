#ifndef PERFCOUNT_HH
#define PERFCOUNT_HH
#include "element.hh"
#include "hashmap.hh"

/*
 * =c
 * PerfCount(index [, metric1 [, metric2]])
 * =d
 * Store information about different Pentium Pro performance metrics in each
 * packet.
 * Each packet can store two metrics in two slots.
 * "index" (either 0 or 1) specifies whether to store the metrics in 
 * slot 0 or 1.
 * By default, "metric1" is "DCU_MISS_OUTSTANDING", and metric2 is 
 * "IFU_IFETCH_MISS".  The defaults can be changed by specifying metrics
 * on the configuration line.
 * Valid metrics are:
 * DCU_MISS_OUTSTANDING,
 * IFU_IFETCH_MISS,
 * L2_IFETCH,
 * L2_LD,
 * L2_RQSTS
 * 
 * =a StorePerf
 */

#define MSR_OS (1<<17)
#define MSR_OCCURRENCE (1<<18)
#define MSR_ENABLE (1<<22)
#define MSR_FLAGS0 (MSR_OS|MSR_OCCURRENCE|MSR_ENABLE)
#define MSR_FLAGS1 (MSR_OS|MSR_OCCURRENCE)

#define MSR_EVNTSEL0 0x186
#define MSR_EVNTSEL1 0x187

class PerfCount : public Element {
  unsigned _idx;
  unsigned _metric0;
  unsigned _metric1;
  
  HashMap<String, unsigned> _metrics;

 public:
  
  PerfCount();
  ~PerfCount();
  
  const char *class_name() const		{ return "PerfCount"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  PerfCount *clone() const;

  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif

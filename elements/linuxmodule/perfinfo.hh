#ifndef PERFINFO_HH
#define PERFINFO_HH
#include <click/element.hh>
#include <click/hashmap.hh>

/*
 * =c
 * PerfInfo([metric1 [, metric2]])
 * =s profiling
 * manipulates Pentium Pro performance counters
 * =d
 * Sets up Pentium Pro performance counters for use by PerfCount.
 * By default, "metric1" is "DCU_MISS_OUTSTANDING", and metric2 is 
 * "IFU_IFETCH_MISS".  The defaults can be changed by specifying metrics
 * on the configuration line.
 * Valid metrics are:
 * DCU_MISS_OUTSTANDING,
 * IFU_FETCH_MISS,
 * IFU_MEM_STALL,
 * IFU_FETCH, 
 * INST_RETIRED,
 * L2_IFETCH,
 * L2_RQSTS,
 * L2_LINES_OUTM,
 * BUS_TRAN_MEM,
 * BUS_TRAN_INVAL
 * 
 * =a PerfCount
 */

#define MSR_OS (1<<17)
#define MSR_OCCURRENCE (1<<18)
#define MSR_ENABLE (1<<22)
#define MSR_FLAGS0 (MSR_OS|MSR_OCCURRENCE|MSR_ENABLE)
#define MSR_FLAGS1 (MSR_OS|MSR_OCCURRENCE)

#define MSR_EVNTSEL0 0x186
#define MSR_EVNTSEL1 0x187

class PerfInfo : public Element {

  static unsigned _init;
  static unsigned _metric0;
  static unsigned _metric1;

 public:
  
  PerfInfo();
  ~PerfInfo();
  PerfInfo *clone() const;
  
  const char *class_name() const		{ return "PerfInfo"; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void uninitialize();
};

#endif

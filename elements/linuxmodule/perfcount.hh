#ifndef PERFCOUNT_HH
#define PERFCOUNT_HH
#include "element.hh"

/*
 * =c
 * PerfCount(index)
 * =d
 * Store information about icache and dcache misses in a packet's annotation.
 * Each packet can store 2 dcache miss counts and 2 icache miss counts.
 * Index (either 0 or 1) specifies whether to store the dcache/icache info
 * in slot 0 or 1.
 */

#define IFU_IFETCH_MISS 0x81
#define DCU_MISS_OUTSTANDING 0x48

#define MSR_OS (1<<17)
#define MSR_OCCURRENCE (1<<18)
#define MSR_ENABLE (1<<22)
#define MSR_FLAGS0 (MSR_OS|MSR_OCCURRENCE|MSR_ENABLE)
#define MSR_FLAGS1 (MSR_OS|MSR_OCCURRENCE)

#define MSR_EVNTSEL0 0x186
#define MSR_EVNTSEL1 0x187

class PerfCount : public Element {
  unsigned int _idx;
  
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

#ifndef PERFCOUNT_HH
#define PERFCOUNT_HH
#include "element.hh"
#include "hashmap.hh"
#include "perfinfo.hh"

/*
 * =c
 * PerfCount(index)
 * =d
 * Stores information about different Pentium Pro performance metrics in each
 * packet.
 * Each packet can store two metrics in two slots.
 * "index" (either 0 or 1) specifies whether to store the metrics in 
 * slot 0 or 1.  When "index" is 1, PerfCount display total and average
 * statistics on the difference between slots 1 and 0 in /proc.
 *
 * Metrics to be measured are determined by PerfInfo.
 * 
 * =a PerfInfo
 */

class PerfCount : public Element {
  unsigned _idx;

 public:
  
  PerfCount();
  ~PerfCount();
  
  const char *class_name() const		{ return "PerfCount"; }
  const char *processing() const		{ return AGNOSTIC; }
  PerfCount *clone() const;
  void add_handlers();

  int configure(const String &, ErrorHandler *);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

  unsigned _packets;
  unsigned _m0;
  unsigned _m1;
};

#endif

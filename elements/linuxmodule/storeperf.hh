#ifndef STOREPERF_HH
#define STOREPERF_HH
#include "element.hh"

/*
 * =c
 * StorePerf()
 * =d
 * Each packet stores 2 icache/dcache miss counts. 
 * StorePerf accumulates the difference between these two counts for all
 * packets it sees, together with a packet count.
 * Total and average statistics are printed in /proc.
 */

class StorePerf : public Element {

 public:
  
  StorePerf();
  ~StorePerf();
  
  const char *class_name() const		{ return "StorePerf"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  StorePerf *clone() const;
  void add_handlers(HandlerRegistry *fcr);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
  // statistics 
  unsigned _packets;
  unsigned _icache;
  unsigned _dcache;
  
};

#endif

#ifndef STORECYCLES_HH
#define STORECYCLES_HH
#include "element.hh"

/*
 * =c
 * StoreCycles(X1,X2)
 * =d
 * Each packet stores 4 cycle counts. StoreCycles accumulates the difference
 * between cycle counts X1 and X2 for all packets, together with a packet
 * count. Total and average statistics are printed in uninitialization time.
 */

class StoreCycles : public Element {
  unsigned short _idx1;
  unsigned short _idx2;
  
 public:
  
  StoreCycles();
  ~StoreCycles();
  
  const char *class_name() const		{ return "StoreCycles"; }
  Processing default_processing() const		{ return AGNOSTIC; }
  StoreCycles *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers(HandlerRegistry *fcr);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
  // statistics 
  unsigned long _sum;
  unsigned long _pkt_cnt;
  
};

#endif

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
  unsigned long long _sum;
  unsigned long long _pkt_cnt;
  
 public:
  
  StoreCycles();
  ~StoreCycles();
  
  const char *class_name() const		{ return "StoreCycles"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  StoreCycles *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif

#ifndef STORECYCLES_HH
#define STORECYCLES_HH
#include <click/element.hh>

/*
 * =c
 * StoreCycles(X1,X2)
 * =s manipulates cycle count annotations
 * V<debugging>
 * =d
 * Each packet stores 4 cycle counts. StoreCycles accumulates the difference
 * between cycle counts X1 and X2 for all packets, together with a packet
 * count. Total and average statistics are printed in /proc.
 * =a CycleCount
 */

class StoreCycles : public Element {
  unsigned short _idx1;
  unsigned short _idx2;
  
 public:
  
  StoreCycles();
  ~StoreCycles();
  
  const char *class_name() const		{ return "StoreCycles"; }
  const char *processing() const		{ return AGNOSTIC; }
  StoreCycles *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
  // statistics 
  unsigned long long _sum;
  unsigned long long _pkt_cnt;
  
};

#endif

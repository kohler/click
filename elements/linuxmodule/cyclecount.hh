#ifndef CYCLECOUNT_HH
#define CYCLECOUNT_HH
#include "element.hh"

/*
 * =c
 * CycleCount(X)
 * =d
 * Assign cycle counter to a packet's cycle counters. Each packet can store 4
 * cycle counts, X specifies which storage to use.
 * =a StoreCycles
 */

class CycleCount : public Element {
  unsigned int _idx;
  
 public:
  
  CycleCount();
  ~CycleCount();
  
  const char *class_name() const		{ return "CycleCount"; }
  const char *processing() const		{ return AGNOSTIC; }
  CycleCount *clone() const;
  int configure(const String &, ErrorHandler *);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif

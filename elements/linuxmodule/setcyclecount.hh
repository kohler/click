#ifndef SETCYCLECOUNT_HH
#define SETCYCLECOUNT_HH

/*
 * =c
 * SetCycleCount()
 * =s stores cycle count in annotation
 * V<debugging>
 * =d
 *
 * Stores the current cycle count in an annotation in each packet. In
 * combination with CycleCountAccum, this lets you measure how many cycles it
 * takes a packet to pass from one point to another.
 *
 * =n
 *
 * A packet has room for either exactly one cycle count or exactly one
 * performance metric.
 * 
 * =a CycleCountAccum, RoundTripCycleCount, SetPerfCount, PerfCountAccum */

#include <click/element.hh>

class SetCycleCount : public Element { public:
  
  SetCycleCount();
  ~SetCycleCount();
  
  const char *class_name() const		{ return "SetCycleCount"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetCycleCount *clone() const;
  
  void push(int, Packet *p);
  Packet *pull(int);

};

#endif

#ifndef RTCYCLES_HH
#define RTCYCLES_HH
#include "element.hh"

/*
 * =c
 * RTCycles()
 *
 * =s measures round trip cycles on a push or pull path.
 * V<debugging>
 * 
 * =d
 * Measures the number of CPU cycles it takes for a push or pull to come back
 * to the element. This is a good indication of how much CPU is spent on the
 * Click path after or before this element.
 *
 * =h cycles read
 * Returns current cycle count.
 *
 * =a CycleCount, StoreCycles
 */

class RTCycles : public Element {
public:
  RTCycles();
  ~RTCycles();
  
  const char *class_name() const		{ return "RTCycles"; }
  const char *processing() const		{ return AGNOSTIC; }
  RTCycles *clone() const			{ return new RTCycles; }
  
  void push(int, Packet *p);
  Packet *pull(int);
  void add_handlers();
  
  unsigned long long _sum;
  unsigned long long _pkts;
};

#endif

#ifndef STRIDESWITCH_HH
#define STRIDESWITCH_HH
#include "elements/standard/stridesched.hh"

/*
 * =c
 * StrideSwitch(TICKETS0, ..., TICKETS<i>N-1</i>)
 * =d
 * Has one input and N outputs.  Performs simple packet-based stride
 * switching, assigning TICKETS<i>i</i> to output <i>i</i> for each input.
 * 
 * Each time a packet arrives on the input, StrideSwitch sends the packet to
 * the next scheduled output according to the stride scheduling algorithm.
 *
 * =h tickets0...tickets<i>N-1</i> read/write
 * Returns or sets the number of tickets for each output port.
 *
 * =a Switch RoundRobinSwitch
 * =a StrideSched
 */

class StrideSwitch : public StrideSched { public:
  
  StrideSwitch();
  ~StrideSwitch();

  const char *class_name() const		{ return "StrideSwitch"; }
  const char *processing() const		{ return PUSH; }
  
  StrideSwitch *clone() const			{ return new StrideSwitch; }
  int configure(const Vector<String> &conf, ErrorHandler *errh);
  
  void push(int, Packet *);

};

#endif

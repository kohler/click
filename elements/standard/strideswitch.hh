#ifndef STRIDESWITCH_HH
#define STRIDESWITCH_HH
#include "elements/standard/stridesched.hh"

/*
 * =c
 * StrideSwitch(TICKETS0, ..., TICKETSI<N-1>)
 * =d
 * Has one input and N outputs.  Performs simple packet-based stride
 * switching, assigning TICKETSI<i> to output I<i> for each input.
 * 
 * Each time a packet arrives on the input, StrideSwitch sends the packet to
 * the next scheduled output according to the stride scheduling algorithm.
 *
 * =h tickets0...ticketsI<N-1> read/write
 * Returns or sets the number of tickets for each output port.
 *
 * =a Switch, RoundRobinSwitch, StrideSched
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

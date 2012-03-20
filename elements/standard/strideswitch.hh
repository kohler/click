#ifndef CLICK_STRIDESWITCH_HH
#define CLICK_STRIDESWITCH_HH
#include "elements/standard/stridesched.hh"
CLICK_DECLS

/*
 * =c
 * StrideSwitch(TICKETS0, ..., TICKETSI<N-1>)
 * =s classification
 * sends packets to stride-scheduled outputs
 * =d
 * Has one input and N outputs.  Performs simple packet-based stride
 * switching, assigning TICKETSI<i> to output I<i>.
 *
 * Each time a packet arrives on the input, StrideSwitch sends the packet to
 * the next scheduled output according to the stride scheduling algorithm.
 *
 * =h tickets0...ticketsI<N-1> read/write
 * Returns or sets the number of tickets for each output port.
 *
 * =a RoundRobinSwitch, Switch, HashSwitch, RandomSwitch, StrideSched
 */

class StrideSwitch : public StrideSched { public:

  StrideSwitch();

  const char *class_name() const		{ return "StrideSwitch"; }
  const char *port_count() const		{ return "1/1-"; }
  const char *processing() const		{ return PUSH; }

  void push(int, Packet *);

};

CLICK_ENDDECLS
#endif

#ifndef RRSWITCH_HH
#define RRSWITCH_HH
#include "element.hh"

/*
 * =c
 * RoundRobinSwitch
 * =s
 * sends packets to round-robin outputs
 * V<classification>
 * =io
 * One input, one or more outputs
 * =d
 * Pushes each arriving packet to one of the N outputs. The next packet
 * will be pushed to the following output in round-robin order.
 *
 * =a Switch, StrideSwitch, RoundRobinSched
 */

class RoundRobinSwitch : public Element {

  int _next;
  
 public:
  
  RoundRobinSwitch();
  ~RoundRobinSwitch();
  
  const char *class_name() const	{ return "RoundRobinSwitch"; }
  const char *processing() const	{ return PUSH; }
  void notify_noutputs(int);
  
  RoundRobinSwitch *clone() const	{ return new RoundRobinSwitch; }
  
  void push(int, Packet *);
  
};

#endif

#ifndef RRSCHED_HH
#define RRSCHED_HH
#include "element.hh"

/*
 * =c
 * RoundRobinSched
 * =s pulls from round-robin inputs V<packet scheduling>
 * =io
 * One output, zero or more inputs
 * =d
 * Each time a pull comes in the output, pulls from its inputs
 * in turn until one produces a packet. When the next pull
 * comes in, it starts from the input after the one that
 * last produced a packet. This amounts to a round robin
 * scheduler.
 *
 * =a PrioSched, StrideSched, RoundRobinSwitch
 */

class RRSched : public Element {
  
 public:
  
  RRSched();
  ~RRSched();
  
  const char *class_name() const		{ return "RoundRobinSched"; }
  const char *processing() const		{ return PULL; }
  void notify_ninputs(int);
  
  RRSched *clone() const			{ return new RRSched; }
  
  Packet *pull(int port);

 private:

  int _next;
  
};

#endif RRSCHED_HH

#ifndef RRSCHED_HH
#define RRSCHED_HH
#include "unlimelement.hh"

/*
 * =c
 * RoundRobinSched()
 * =d
 * Each time a pull comes in the output, pulls from its inputs
 * in turn until one produces a packet. When the next pull
 * comes in, it starts from the input after the one that
 * last produced a packet. This amounts to a round robin
 * scheduler.
 * 
 * =a PrioSched
 * =a StrideSched
 */

class RRSched : public UnlimitedElement {
  
 public:
  
  RRSched();
  ~RRSched();
  
  const char *class_name() const		{ return "RoundRobinSched"; }
  const char *processing() const		{ return PULL; }
  
  bool unlimited_inputs() const			{ return true; }
  
  RRSched *clone() const			{ return new RRSched; }
  
  Packet *pull(int port);

private:

  int _next;
  
};

#endif RRSCHED_HH

#ifndef PRIOSCHED_HH
#define PRIOSCHED_HH
#include <click/element.hh>

/*
 * =c
 * PrioSched
 * =s pulls from priority-scheduled inputs
 * V<packet scheduling>
 * =d
 * Each time a pull comes in the output, PrioSched pulls from
 * each of the inputs starting from input 0.
 * The packet from the first successful pull is returned.
 * This amounts to a strict priority scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 *
 * =a Queue, RoundRobinSched, StrideSched
 */

class PrioSched : public Element {
  
 public:
  
  PrioSched();
  ~PrioSched();
  
  const char *class_name() const		{ return "PrioSched"; }
  const char *processing() const		{ return PULL; }
  void notify_ninputs(int);
  
  PrioSched *clone() const			{ return new PrioSched; }
  
  Packet *pull(int port);
  
};

#endif PRIOSCHED_HH

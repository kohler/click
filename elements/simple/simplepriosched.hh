// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SIMPLEPRIOSCHED_HH
#define CLICK_SIMPLEPRIOSCHED_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SimplePrioSched
 * =s packet scheduling
 * pulls from priority-scheduled inputs
 * =d
 * Each time a pull comes in the output, SimplePrioSched pulls from
 * each of the inputs starting from input 0.
 * The packet from the first successful pull is returned.
 * This amounts to a strict priority scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * SimplePrioSched does not use notification.
 *
 * =a PrioSched, Queue, RoundRobinSched, StrideSched, DRRSched
 */

class SimplePrioSched : public Element { public:
  
    SimplePrioSched();
    ~SimplePrioSched();
  
    const char *class_name() const	{ return "SimplePrioSched"; }
    const char *processing() const	{ return PULL; }
    PrioSched *clone() const		{ return new SimplePrioSched; }

    void notify_ninputs(int);
  
    Packet *pull(int port);
  
};

CLICK_ENDDECLS
#endif

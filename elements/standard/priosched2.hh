// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PRIOSCHED2_HH
#define CLICK_PRIOSCHED2_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * PrioSched2
 * =s packet scheduling
 * pulls from priority-scheduled inputs
 * =d
 * Each time a pull comes in the output, PrioSched2 pulls from
 * each of the inputs starting from input 0.
 * The packet from the first successful pull is returned.
 * This amounts to a strict priority scheduler.
 * PrioSched2 uses notification. (PrioSched doesn't.)
 *
 * The inputs usually come from Queues or other pull schedulers.
 *
 * =a Queue, RoundRobinSched, StrideSched, DRRSched
 */

class PrioSched2 : public Element { public:
  
    PrioSched2();
    ~PrioSched2();
  
    const char *class_name() const		{ return "PrioSched2"; }
    const char *processing() const		{ return PULL; }
    PrioSched2 *clone() const			{ return new PrioSched2; }
  
    void notify_ninputs(int);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
  
    Packet *pull(int port);

  private:
    
    NotifierSignal *_signals;
    
};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RRSCHED_HH
#define CLICK_RRSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * RoundRobinSched
 * =s packet scheduling
 * pulls from round-robin inputs
 * =io
 * One output, zero or more inputs
 * =d
 * Each time a pull comes in the output, pulls from its inputs
 * in turn until one produces a packet. When the next pull
 * comes in, it starts from the input after the one that
 * last produced a packet. This amounts to a round robin
 * scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * RoundRobinSched uses notification to avoid pulling from empty inputs.
 *
 * =a PrioSched, StrideSched, DRRSched, RoundRobinSwitch, SimpleRoundRobinSched
 */

class RRSched : public Element { public:
  
    RRSched();
    ~RRSched();
  
    const char *class_name() const	{ return "RoundRobinSched"; }
    const char *processing() const	{ return PULL; }
  
    void notify_ninputs(int);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
  
    Packet *pull(int port);

  private:

    int _next;
    NotifierSignal *_signals;
  
};

CLICK_ENDDECLS
#endif

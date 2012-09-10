// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PRIOSCHED_HH
#define CLICK_PRIOSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * PrioSched
 * =s scheduling
 * pulls from priority-scheduled inputs
 * =d
 * Each time a pull comes in the output, PrioSched pulls from
 * each of the inputs starting from input 0.
 * The packet from the first successful pull is returned.
 * This amounts to a strict priority scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * PrioSched uses notification to avoid pulling from empty inputs.
 *
 * =a Queue, RoundRobinSched, StrideSched, DRRSched, SimplePrioSched
 */

class PrioSched : public Element { public:

    PrioSched() CLICK_COLD;

    const char *class_name() const	{ return "PrioSched"; }
    const char *port_count() const	{ return "-/1"; }
    const char *processing() const	{ return PULL; }
    const char *flags() const		{ return "S0"; }

    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    Packet *pull(int port);

  private:

    NotifierSignal *_signals;

};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SIMPLEPRIOSCHED_HH
#define CLICK_SIMPLEPRIOSCHED_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SimplePrioSched
 * =s scheduling
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

    SimplePrioSched() CLICK_COLD;
    ~SimplePrioSched() CLICK_COLD;

    const char *class_name() const	{ return "SimplePrioSched"; }
    const char *port_count() const	{ return "-/1"; }
    const char *processing() const	{ return PULL; }
    const char *flags() const		{ return "S0"; }

    Packet *pull(int port);

};

CLICK_ENDDECLS
#endif

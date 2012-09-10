// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SIMPLERRSCHED_HH
#define CLICK_SIMPLERRSCHED_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SimpleRoundRobinSched
 * =s scheduling
 * pulls from round-robin inputs
 * =io
 * one output, zero or more inputs
 * =d
 * Each time a pull comes in the output, pulls from its inputs
 * in turn until one produces a packet. When the next pull
 * comes in, it starts from the input after the one that
 * last produced a packet. This amounts to a round robin
 * scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * SimpleRoundRobinSched does not use notification.
 *
 * =a RoundRobinSched, PrioSched, StrideSched, DRRSched, RoundRobinSwitch
 */

class SimpleRRSched : public Element { public:

    SimpleRRSched() CLICK_COLD;
    ~SimpleRRSched() CLICK_COLD;

    const char *class_name() const	{ return "SimpleRoundRobinSched"; }
    const char *port_count() const	{ return "-/1"; }
    const char *processing() const	{ return PULL; }
    const char *flags() const		{ return "S0"; }

    Packet *pull(int port);

  private:

    int _next;

};

CLICK_ENDDECLS
#endif

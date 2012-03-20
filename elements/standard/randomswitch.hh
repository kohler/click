// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RANDOMSWITCH_HH
#define CLICK_RANDOMSWITCH_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * RandomSwitch
 * =s classification
 * sends packets to random outputs
 * =io
 * one input, one or more outputs
 * =d
 * Pushes each arriving packet to one of the N outputs, choosing outputs randomly.
 *
 * =a Switch, StrideSwitch, RoundRobinSwitch, HashSwitch
 */

class RandomSwitch : public Element { public:

    RandomSwitch();

    const char *class_name() const	{ return "RandomSwitch"; }
    const char *port_count() const	{ return "1/1-"; }
    const char *processing() const	{ return PUSH; }

    void push(int, Packet *);

};

CLICK_ENDDECLS
#endif

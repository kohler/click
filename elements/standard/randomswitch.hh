// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RANDOMSWITCH_HH
#define CLICK_RANDOMSWITCH_HH
#include <click/element.hh>
#include <click/atomic.hh>

/*
 * =c
 * RandomSwitch
 * =s classification
 * sends packets to random outputs
 * =io
 * One input, one or more outputs
 * =d
 * Pushes each arriving packet to one of the N outputs, choosing outputs randomly.
 *
 * =a Switch, StrideSwitch, RoundRobinSwitch, HashSwitch
 */

class RandomSwitch : public Element { public:
  
    RandomSwitch();
    ~RandomSwitch();

    const char *class_name() const	{ return "RandomSwitch"; }
    const char *processing() const	{ return PUSH; }
    RandomSwitch *clone() const		{ return new RandomSwitch; }
    void notify_noutputs(int);
  
    void push(int, Packet *);
  
};

#endif

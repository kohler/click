// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAD_HH
#define CLICK_PAD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Pad

=s basicmod

*/

class Pad : public Element { public:

    Pad() CLICK_COLD;
    ~Pad() CLICK_COLD;

    const char *class_name() const		{ return "Pad"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif

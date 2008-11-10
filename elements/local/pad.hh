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

    Pad();
    ~Pad();

    const char *class_name() const		{ return "Pad"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif

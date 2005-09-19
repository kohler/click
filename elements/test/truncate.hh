// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TRUNCATE_HH
#define CLICK_TRUNCATE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * Truncate(MAX)
 * =s encapsulation
 * strips bytes from the back of packets
 * =d
 * Deletes the last MAX bytes from each packet.
 * =a Strip
 */

class Truncate : public Element { public:

    Truncate();
    ~Truncate();

    const char *class_name() const		{ return "Truncate"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);

  private:

    unsigned _nbytes;

};

CLICK_ENDDECLS
#endif

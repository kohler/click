// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_STRIP_HH
#define CLICK_STRIP_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * Strip(N)
 * =s encapsulation
 * strips bytes from front of packets
 * =d
 * Deletes the first N bytes from each packet.
 * =e
 * Use this to get rid of the Ethernet header:
 *
 *   Strip(14)
 * =a StripToNetworkHeader, EtherEncap, IPEncap
 */

class Strip : public Element { public:

    Strip();
    ~Strip();

    const char *class_name() const		{ return "Strip"; }
    const char *processing() const		{ return AGNOSTIC; }
    Strip *clone() const			{ return new Strip; }

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);

  private:

    unsigned _nbytes;

};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_NEIGHBORHOODTEST_HH
#define CLICK_NEIGHBORHOODTEST_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

NeighborhoodTest()

=s test

check ElementNeighborhoodVisitor functionality

=d

NeighborhoodTest provides handlers named "upstream" and "downstream" that,
when read, return the calculated upstream or downstream neighborhood, to the
diameter specified as the read parameter.  "upstream0" through "upstreamN"
evaluate upstream neighborhoods for individual input ports, and similarly for
"downstream0" through "downstreamN".
*/

class NeighborhoodTest : public Element { public:

    NeighborhoodTest();

    const char *class_name() const		{ return "NeighborhoodTest"; }
    const char *port_count() const		{ return "-/-"; }
    const char *flow_code() const		{ return "x/y"; }

    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    static int handler(int operation, String &data, Element *element,
		       const Handler *handler, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif

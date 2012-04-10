// -*- c-basic-offset: 4 -*-
#ifndef CLICK_VECTORTEST_HH
#define CLICK_VECTORTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

VectorTest()

=s test

runs regression tests for Vector

=d

VectorTest runs Vector regression tests at initialization time. It
does not route packets.

*/

class VectorTest : public Element { public:

    VectorTest();

    const char *class_name() const		{ return "VectorTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

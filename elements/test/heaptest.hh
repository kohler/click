// -*- c-basic-offset: 4 -*-
#ifndef CLICK_HEAPTEST_HH
#define CLICK_HEAPTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

HeapTest()

=s test

runs regression tests for heap functions

=d

HeapTest runs regression tests for Click's heap functions at initialization
time. It does not route packets.

*/

class HeapTest : public Element { public:

    HeapTest() CLICK_COLD;

    const char *class_name() const		{ return "HeapTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

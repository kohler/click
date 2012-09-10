// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BITVECTORTEST_HH
#define CLICK_BITVECTORTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

BitvectorTest()

=s test

runs regression tests for Bitvector

=d

BitvectorTest runs Bitvector regression tests at initialization time. It
does not route packets.

*/

class BitvectorTest : public Element { public:

    BitvectorTest() CLICK_COLD;

    const char *class_name() const		{ return "BitvectorTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

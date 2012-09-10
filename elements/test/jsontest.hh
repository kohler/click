// -*- c-basic-offset: 4 -*-
#ifndef CLICK_JSONTEST_HH
#define CLICK_JSONTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

JsonTest()

=s test

runs regression tests for Json

=d

JsonTest runs Json regression tests at initialization time. It
does not route packets.

*/

class JsonTest : public Element { public:

    JsonTest() CLICK_COLD;

    const char *class_name() const		{ return "JsonTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

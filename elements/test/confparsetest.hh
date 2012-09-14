// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CONFPARSETEST_HH
#define CLICK_CONFPARSETEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ConfParseTest()

=s test

run regression tests for configuration parsing

=d

ConfParseTest runs configuration parsing regression tests at initialization
time. It does not route packets.

*/

class ConfParseTest : public Element { public:

    ConfParseTest() CLICK_COLD;

    const char *class_name() const		{ return "ConfParseTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

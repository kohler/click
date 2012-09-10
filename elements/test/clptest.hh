// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CLPTEST_HH
#define CLICK_CLPTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CLPTest([keywords])

=s test

runs regression tests for CLP command line parser

=d

CLPTest runs regression tests for the CLP command line parsing library at
initialization time. It does not route packets.

*/

class CLPTest : public Element { public:

    CLPTest() CLICK_COLD;

    const char *class_name() const		{ return "CLPTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

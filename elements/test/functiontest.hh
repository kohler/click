// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FUNCTIONTEST_HH
#define CLICK_FUNCTIONTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

FunctionTest()

=s test

runs regression tests for other Click functions

=d

FunctionTest runs regression tests for other Click functions, such as for
integer functions, at initialization time. It does not route packets.

*/

class FunctionTest : public Element { public:

    FunctionTest();

    const char *class_name() const		{ return "FunctionTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

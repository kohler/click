// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ERRORTEST_HH
#define CLICK_ERRORTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ErrorTest()

=s test

runs regression tests for error handling

=d

ErrorTest runs error handling regression tests at initialization
time.

*/

class ErrorTest : public Element { public:

    ErrorTest() CLICK_COLD;

    const char *class_name() const		{ return "ErrorTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

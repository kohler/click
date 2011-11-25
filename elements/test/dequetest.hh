// -*- c-basic-offset: 4 -*-
#ifndef CLICK_DEQUETEST_HH
#define CLICK_DEQUETEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

DequeTest()

=s test

runs regression tests for Deque

=d

DequeTest runs Deque regression tests at initialization time. It does not
route packets.

*/

class DequeTest : public Element { public:

    DequeTest();
    ~DequeTest();

    const char *class_name() const		{ return "DequeTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

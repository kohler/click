// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TIMERTEST_HH
#define CLICK_TIMERTEST_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

TimerTest()

=s test

runs regression tests for Timer

=d

TimerTest runs regression tests for Click's Timer class at initialization
time. It does not route packets.

*/

class TimerTest : public Element { public:

    TimerTest();
    ~TimerTest();

    const char *class_name() const		{ return "TimerTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

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

Keyword arguments are:

=over 8

=item BENCHMARK

Integer.  If set to a positive number, then TimerTest runs a timer
manipulation benchmark at installation time involving BENCHMARK total
timers.  Default is 0 (don't benchmark).

=back

*/

class TimerTest : public Element { public:

    TimerTest();
    ~TimerTest();

    const char *class_name() const		{ return "TimerTest"; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

  private:

    int _benchmark;

    void benchmark_schedules(Timer *ts, int nts, const Timestamp &now);
    void benchmark_changes(Timer *ts, int nts, const Timestamp &now);
    void benchmark_fires(Timer *ts, int nts, const Timestamp &now);

};

CLICK_ENDDECLS
#endif

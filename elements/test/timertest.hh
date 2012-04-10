// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TIMERTEST_HH
#define CLICK_TIMERTEST_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

TimerTest([I<keywords>])

=s test

runs regression tests for Timer

=d

Without other arguments, TimerTest runs regression tests for Click's Timer
class at initialization time.

TimerTest includes a timer which prints a message when scheduled. This timer
is controlled by the DELAY and/or SCHEDULED keyword arguments and several
handlers.

TimerTest does not route packets.

Keyword arguments are:

=over 8

=item DELAY

Timestamp. If set, TimerTest schedules a timer starting DELAY seconds in the
future. On expiry, a message such as "C<1000000000.010000: t1 :: TimerTest fired>"
is printed to standard error.

=item BENCHMARK

Integer.  If set to a positive number, then TimerTest runs a timer
manipulation benchmark at installation time involving BENCHMARK total
timers.  Default is 0 (don't benchmark).

=back

=h scheduled rw

Boolean. Returns whether the TimerTest's timer is scheduled.

=h expiry r

Timestamp. Returns the expiration time for the TimerTest's timer, if any.

=h schedule_after w

Schedule the TimerTest's timer to fire after a given time.

=h unschedule w

Unschedule the TimerTest's timer.

*/

class TimerTest : public Element { public:

    TimerTest();

    const char *class_name() const		{ return "TimerTest"; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);
    void add_handlers();

    void run_timer(Timer *t);

  private:

    Timer _timer;
    int _benchmark;

    void benchmark_schedules(Timer *ts, int nts, const Timestamp &now);
    void benchmark_changes(Timer *ts, int nts, const Timestamp &now);
    void benchmark_fires(Timer *ts, int nts, const Timestamp &now);

    enum { h_scheduled, h_expiry, h_schedule_after, h_unschedule };
    static String read_handler(Element *e, void *user_data);
    static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif

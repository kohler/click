// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TASKTHREADTEST_HH
#define CLICK_TASKTHREADTEST_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

TaskThreadTest([I<keywords>])

=s test

runs regression tests for Timer

=d

Without other arguments, TaskThreadTest runs regression tests for Click's Timer
class at initialization time.

TaskThreadTest includes a timer which prints a message when scheduled. This timer
is controlled by the DELAY and/or SCHEDULED keyword arguments and several
handlers.

TaskThreadTest does not route packets.

Keyword arguments are:

=over 8

=item DELAY

Timestamp. If set, TaskThreadTest schedules a timer starting DELAY seconds in the
future. On expiry, a message such as "C<1000000000.010000: t1 :: TaskThreadTest fired>"
is printed to standard error.

=item BENCHMARK

Integer.  If set to a positive number, then TaskThreadTest runs a timer
manipulation benchmark at installation time involving BENCHMARK total
timers.  Default is 0 (don't benchmark).

=back

=h scheduled rw

Boolean. Returns whether the TaskThreadTest's timer is scheduled.

=h expiry r

Timestamp. Returns the expiration time for the TaskThreadTest's timer, if any.

=h schedule_after w

Schedule the TaskThreadTest's timer to fire after a given time.

=h unschedule w

Unschedule the TaskThreadTest's timer.

*/

class TaskThreadTest : public Element { public:
    TaskThreadTest() CLICK_COLD;

    const char* class_name() const		{ return "TaskThreadTest"; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;

    bool run_task(Task* t);

  private:
    Task _main_task;
    Task* _tasks;
    unsigned _ntasks;
    unsigned _free_batch;
    unsigned _change_batch;

    static bool main_task_callback(Task*, void*);
};

CLICK_ENDDECLS
#endif

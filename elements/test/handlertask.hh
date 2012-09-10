// -*- c-basic-offset: 4 -*-
#ifndef CLICK_HANDLERTASK_HH
#define CLICK_HANDLERTASK_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/handlercall.hh>
CLICK_DECLS

/*
=c

HandlerTask(HANDLER, [<keyword> LIMIT, STOP, ACTIVE])

=s test

associated with a do-nothing Task

=d

HandlerTask simply schedule a task which, when scheduled, does nothing.
This can be useful for benchmarking.

=over 8

=item ACTIVE

Boolean.  If false, HandlerTask will not schedule itself at initialization
time.  Use the C<scheduled> write handler to schedule the task later.  Default
is true.

=item RESCHEDULE

Boolean.  If true, HandlerTask will reschedule itself each time it runs.
Default is false.

=back

=h count r

Returns the number of times the element has been scheduled.

=a

ScheduleInfo
*/

class HandlerTask : public Element { public:

    HandlerTask() CLICK_COLD;

    const char *class_name() const		{ return "HandlerTask"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);

  private:

    Task _task;
    HandlerCall _h;
    uint32_t _count;
    bool _active;
    bool _reschedule;

};

CLICK_ENDDECLS
#endif

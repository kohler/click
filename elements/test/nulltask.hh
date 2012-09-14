// -*- c-basic-offset: 4 -*-
#ifndef CLICK_NULLTASK_HH
#define CLICK_NULLTASK_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

NullTask([<keyword> LIMIT, STOP, ACTIVE])

=s test

associated with a do-nothing Task

=d

NullTask simply schedule a task which, when scheduled, does nothing.
This can be useful for benchmarking.

=over 8

=item LIMIT

Unsigned.  NullTask will schedule itself at most LIMIT times.  0 means
forever.  Default is 0.

=item STOP

Boolean.  If true, NullTask will stop the driver when LIMIT is
reached.  Default is false.

=item ACTIVE

Boolean.  If false, NullTask will not schedule itself at initialization time.
Use the C<scheduled> write handler to schedule the task later.  Default is
true.

=back

=h count r

Returns the number of times the element has been scheduled.

=h reset w

Resets the count to 0.

=a

ScheduleInfo
*/

class NullTask : public Element { public:

    NullTask() CLICK_COLD;

    const char *class_name() const		{ return "NullTask"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);

  private:

    uint32_t _count;
    uint32_t _limit;
    Task _task;
    bool _stop;
    bool _active;

    static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

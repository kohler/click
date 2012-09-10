// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SCHEDORDERTEST_HH
#define CLICK_SCHEDORDERTEST_HH
#include <click/element.hh>
#include <click/task.hh>
#include "elements/standard/simplequeue.hh"
CLICK_DECLS

/*
=c

SchedOrderTest(ID, [<keyword> SIZE, LIMIT, STOP])

=s test

remembers scheduling order

=d

SchedOrderTest elements repeatedly schedule themselves, and keep track of the
order in which they were scheduled.  ID is an integer used to distinguish
between different SchedOrderTest elements.  The "order" handler reports the
sequence of IDs corresponding to the order in which the first SIZE
SchedOrderTest tasks were scheduled.  Use ScheduleInfo to set SchedOrderTest's
tickets.

=over 8

=item SIZE

Integer.  The maximum length of the stored ID sequence.  Default is 1024.

=item LIMIT

Unsigned.  SchedOrderTest will schedule itself at most LIMIT times.  0 means
forever.  Default is 0.

=item STOP

Boolean.  If true, SchedOrderTest will stop the driver when the ID sequence is
full.  (Note that this has to do with SIZE, not LIMIT.)  Default is false.

=back

=h order read-only

Reports the ID sequence as a space-separated list of integers.

=a

ScheduleInfo
*/

class SchedOrderTest : public Element { public:

    SchedOrderTest() CLICK_COLD;
    ~SchedOrderTest() CLICK_COLD;

    const char *class_name() const		{ return "SchedOrderTest"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);

  private:

    int _id;
    uint32_t _count;
    uint32_t _limit;
    int** _bufpos_ptr;
    int* _buf_begin;
    int* _bufpos;
    int* _buf_end;
    int _bufsiz;
    Task _task;
    bool _stop;

    static String read_handler(Element*, void*) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

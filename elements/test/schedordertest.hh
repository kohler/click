// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SCHEDORDERTEST_HH
#define CLICK_SCHEDORDERTEST_HH
#include <click/element.hh>
#include <click/task.hh>
#include "elements/standard/simplequeue.hh"
CLICK_DECLS

/*
=c

SchedOrderTest(ID, [<keyword> SIZE])

=s test

remembers scheduling order

=d

=over 8

=item SIZE

Integer.

=item LIMIT

Unsigned.

=item STOP

Boolean.

=h order read-only

*/

class SchedOrderTest : public Element { public:

    SchedOrderTest();
    ~SchedOrderTest();

    const char *class_name() const		{ return "SchedOrderTest"; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();

    bool run_task();

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

    static String read_handler(Element*, void*);
    
};

CLICK_ENDDECLS
#endif

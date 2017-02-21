// -*- c-basic-offset: 4 -*-
#ifndef CLICK_MTDIETEST_HH
#define CLICK_MTDIETEST_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

MTDieTest([I<keywords>])

=s test

runs regression tests for click_rcu

*/

class MTDieTest : public Element { public:

    MTDieTest() CLICK_COLD;

    const char *class_name() const		{ return "MTDieTest"; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    bool run_task(Task *t);

  private:
	Vector<Task*> _task;
	int _nthreads;
};

CLICK_ENDDECLS
#endif

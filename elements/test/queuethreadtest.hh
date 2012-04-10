// -*- c-basic-offset: 4 -*-
#ifndef CLICK_QUEUETHREADTEST_HH
#define CLICK_QUEUETHREADTEST_HH
#include <click/element.hh>
#include "elements/standard/simplequeue.hh"
#include <pthread.h>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

QueueThreadTest1()

=s test

runs regression tests for Queue threading

=e

  QueueThreadTest1 -> Queue -> QueueThreadTest2

*/

class QueueThreadTest1 : public Element { public:

    QueueThreadTest1();

    const char *class_name() const		{ return "QueueThreadTest1"; }
    const char *port_count() const		{ return PORTS_0_1; }

    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

  private:

    pthread_t _push_thread;

};



/*
=c

QueueThreadTest2()

=s test

runs regression tests for Queue threading

=e

  QueueThreadTest1 -> Queue -> QueueThreadTest2

*/

class QueueThreadTest2 : public Element { public:

    QueueThreadTest2();

    const char *class_name() const		{ return "QueueThreadTest2"; }
    const char *port_count() const		{ return PORTS_1_0; }
    const char *processing() const		{ return PULL; }

    int initialize(ErrorHandler *);
    bool run_task(Task *);

  private:

    Task _task;
    uint32_t _next;
    uint32_t _last_msg;
    NotifierSignal _signal;

};

CLICK_ENDDECLS
#endif

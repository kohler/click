#ifndef CLICK_DELAYUNQUEUE_HH
#define CLICK_DELAYUNQUEUE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

DelayUnqueue(DELAY)

=s shaping

delay-inducing pull-to-push converter

=d

Pulls packets from the single input port. Delays them for at least DELAY
seconds, with microsecond precision. A packet with timestamp T will be emitted
no earlier than time (T + DELAY). On output, the packet's timestamp is set to
the delayed time.

DelayUnqueue listens for upstream notification, such as that available from
Queue.

=h delay read/write

Return or set the DELAY parameter.

=a Queue, Unqueue, RatedUnqueue, BandwidthRatedUnqueue, LinkUnqueue,
DelayShaper, SetTimestamp */

class DelayUnqueue : public Element { public:

    DelayUnqueue();

    const char *class_name() const	{ return "DelayUnqueue"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL_TO_PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    bool run_task(Task *);

  private:

    Packet *_p;
    Timestamp _delay;
    Task _task;
    Timer _timer;
    NotifierSignal _signal;

};

CLICK_ENDDECLS
#endif

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

=s packet scheduling

delay inducing pull-to-push converter

=d

Pulls packets from the single input port. Delays them for at least DELAY
seconds, with microsecond precision. A packet with timestamp T will be emitted
no earlier than time (T + DELAY). On output, the packet's timestamp is set to
the current time.

DelayUnqueue listens for upstream notification, such as that available from
Queue.

=a Queue, Unqueue, RatedUnqueue, BandwidthRatedUnqueue, LinkUnqueue,
DelayShaper, SetTimestamp */

class DelayUnqueue : public Element { public:
  
    DelayUnqueue();
    ~DelayUnqueue();

    const char *class_name() const	{ return "DelayUnqueue"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL_TO_PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
  
    bool run_task();
    static String read_param(Element *e, void *);

  private:

    Packet *_p;
    Timestamp _delay;
    Task _task;
    Timer _timer;
    NotifierSignal _signal;
  
};

CLICK_ENDDECLS
#endif

#ifndef CLICK_DISCARD_HH
#define CLICK_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

Discard([I<keywords> ACTIVE, BURST])

=s basicsources

drops all packets

=d

Discards all packets received on its single input. If used in a pull context,
it initiates pulls whenever packets are available, and listens for activity
notification, such as that available from Queue.

Keyword arguments are:

=over 8

=item ACTIVE

Boolean. If false, then Discard does not pull packets. Default is true.
Only meaningful in pull context.

=item BURST

Unsigned. Number of packets to pull per scheduling. Default is 1. Only
meaningful in pull context.

=back

=h active rw

Returns or sets the ACTIVE parameter.  Only present if Discard is in pull
context.

=h count read-only

Returns the number of packets discarded.

=h reset_counts write-only

Resets "count" to 0.

=a Queue */

class Discard : public Element { public:

    Discard();

    const char *class_name() const		{ return "Discard"; }
    const char *port_count() const		{ return PORTS_1_0; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);
    void add_handlers();

    void push(int, Packet *);
    bool run_task(Task *);

  protected:

    Task _task;
    NotifierSignal _signal;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    unsigned _burst;
    bool _active;

    enum { h_reset_counts = 0, h_active = 1 };
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif

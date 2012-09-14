// -*- c-basic-offset: 4 -*-
#ifndef CLICK_DELAYSHAPER_HH
#define CLICK_DELAYSHAPER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

DelayShaper(DELAY)

=s shaping

shapes traffic to meet delay requirements

=d

Pulls packets from the single input port. Delays them for at least DELAY
seconds, with microsecond precision. A packet with timestamp T will be emitted
no earlier than time (T + DELAY). On output, the packet's timestamp is set to
the current time.

Differs from DelayUnqueue in that both its input and output are pull. Packets
being delayed are enqueued until the necessary time has passed. At most one
packet is enqueued at a time. DelayUnqueue returns null for every pull request
until the enqueued packet is ready.

SetTimestamp element can be used to stamp the packet.

=h delay read/write

Returns or sets the DELAY parameter.

=a BandwidthShaper, DelayUnqueue, SetTimestamp */

class DelayShaper : public Element, public ActiveNotifier { public:

    DelayShaper() CLICK_COLD;

    const char *class_name() const	{ return "DelayShaper"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PULL; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *pull(int);
    void run_timer(Timer *);

  private:

    Packet *_p;
    Timestamp _delay;
    Timer _timer;
    NotifierSignal _upstream_signal;
    ActiveNotifier _notifier;

    static String read_param(Element *, void *) CLICK_COLD;
    static int write_param(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

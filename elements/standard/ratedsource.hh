#ifndef CLICK_RATEDSOURCE_HH
#define CLICK_RATEDSOURCE_HH
#include <click/element.hh>
#include <click/tokenbucket.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

RatedSource([DATA, RATE, LIMIT, ACTIVE, I<KEYWORDS>])

=s basicsources

generates packets at specified rate

=d

Creates packets consisting of DATA, emitting at most LIMIT such packets out
its single output at a rate of RATE packets per second. When used as a push
element, RatedSource will send a maximum of one packet per scheduling, so very
high RATEs may not be achievable. If LIMIT is negative, sends packets forever.
Will send packets only if ACTIVE is true. Default DATA is at least 64 bytes
long. Default RATE is 10. Default LIMIT is -1 (send packets forever). Default
ACTIVE is true.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item LENGTH

Integer. If set, the outgoing packet will have this length.

=item RATE

Integer. Same as the RATE argument.

=item BANDWIDTH

Integer. Sets the RATE argument based on the initial outgoing packet length
and a target bandwdith.

=item LIMIT

Integer. Same as the LIMIT argument.

=item ACTIVE

Boolean. Same as the ACTIVE? argument.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=back

To generate a particular repeatable traffic pattern, use this element's
B<rate> and B<active> handlers in conjunction with Script.

=e

  RatedSource(\<0800>, 10, 1000) -> Queue -> ...

=h count read-only
Returns the total number of packets that have been generated.
=h reset write-only
Resets the number of generated packets to 0. The RatedSource will then
generate another LIMIT packets (if it is active).
=h data read/write
Returns or sets the DATA parameter.
=h length read/write
Returns or sets the LENGTH parameter.
=h rate read/write
Returns or sets the RATE parameter.
=h limit read/write
Returns or sets the LIMIT parameter. Negative numbers mean no limit.
=h active read/write
Makes the element active or inactive.

=a

InfiniteSource, Script */

class RatedSource : public Element { public:

    RatedSource();

    const char *class_name() const		{ return "RatedSource"; }
    const char *port_count() const		{ return PORTS_0_1; }
    void add_handlers();

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);
    void cleanup(CleanupStage);

    bool run_task(Task *task);
    Packet *pull(int);

  protected:

    static const unsigned NO_LIMIT = 0xFFFFFFFFU;

    TokenBucket _tb;
    unsigned _count;
    unsigned _limit;
    int _datasize;
    bool _active;
    bool _stop;
    Packet *_packet;
    Task _task;
    Timer _timer;
    String _data;

    void setup_packet();

    static String read_param(Element *, void *);
    static int change_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif

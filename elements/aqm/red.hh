// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_RED_HH
#define CLICK_RED_HH
#include <click/element.hh>
#include <click/ewma.hh>
CLICK_DECLS
class Storage;

/*
=c

RED(MIN_THRESH, MAX_THRESH, MAX_P [, I<KEYWORDS>])

=s aqm

drops packets according to P<RED>

=d

Implements the Random Early Detection packet dropping
algorithm.

A RED element is associated with one or more Storage elements (usually
Queues). It maintains a running average of the sum of the queue lengths, and
marks packets with a probability proportional to that sum. By default, the
Queues are found with flow-based router context. If the RED is a push element,
it uses the nearest downstream Queues; if it is a pull element, it uses the
nearest upstream Queues.

Marked packets are dropped, or emitted on output 1 if RED has two output
ports.

Arguments are:

=over 8

=item MIN_THRESH

Integer.  When the average queue length less than or equal to MIN_THRESH,
input packets are never marked.

=item MAX_THRESH

Integer; must be greater than or equal to MIN_THRESH.  When the average queue
length equals MAX_THRESH, input packets are marked with probability MAX_P.
When the average queue length is between MIN_THRESH and MAX_THRESH, input
packets are marked with probability linearly varying from 0 to MAX_P.  For
behavior above MAX_THRESH, see the SIMPLE argument.

=item MAX_P

Real number between 0 and 1.  The probability of dropping a packet if the
average queue length equals MAX_THRESH.

=item QUEUES

This argument is a space-separated list of Storage element names. RED will use
those elements' queue lengths, rather than any elements found via flow-based
router context.

=item STABILITY

Unsigned. This number determines how stable the average queue size is -- that
is, how quickly it changes due to fluctuations in the instantaneous queue
size. Higher numbers mean more stability. The corresponding conventional RED
parameter is w_q; STABILITY equals -log_2(w_q).

STABILITY should equal

   -log_2 (1 - e^(-1/K)),

where K is the link bandwidth in packets per second. Default STABILITY is 4.
This is very low (unstable) for most purposes; it corresponds to a link
bandwidth of roughly 15 packets per second, or a w_q of 0.25. The NS default
setting for w_q is 0.002, corresponding to a STABILITY of roughly 9.

A STABILITY of 0 means always use the instantaneous queue length.

=item GENTLE

Boolean.  If true (the default), implement the Gentle P<RED> variant first
proposed by Sally Floyd in October 1997.  In this variant, when the average
queue length is between MAX_THRESH and 2*MAX_THRESH, input packets are dropped
with probability linearly varying from MAX_P to 100%; at lengths above
2*MAX_THRESH all packets are dropped.  If GENTLE is false, then at lengths
above MAX_THRESH all packets are dropped.

=back


=e

  ... -> RED(5, 50, 0.02) -> Queue(200) -> ...

=h min_thresh read/write

Returns or sets the MIN_THRESH configuration parameter.

=h max_thresh read/write

Returns or sets the MAX_THRESH configuration parameter.

=h max_p read/write

Returns or sets the MAX_P configuration parameter.

=h drops read-only

Returns the number of packets dropped so far.

=h queues read-only

Returns the Queues associated with this RED element, listed one per line.

=h avg_queue_size read-only

Returns the current average queue size.

=h stats read-only

Returns some human-readable statistics.

=a AdaptiveRED, Queue

Sally Floyd and Van Jacobson. I<Random Early Detection Gateways for
Congestion Avoidance>. ACM Transactions on Networking, B<1>(4), August
1993, pp 397-413.

Sally Floyd. "Optimum functions for computing the drop
probability", October 1997. L<http://www.icir.org/floyd/REDfunc.txt>. */

class RED : public Element { public:

    // Queue sizes are shifted by this much.
    enum { QUEUE_SCALE = 10 };

    typedef DirectEWMAX<StabilityEWMAXParameters<QUEUE_SCALE, uint64_t, int64_t> > ewma_type;

    RED() CLICK_COLD;
    ~RED() CLICK_COLD;

    const char *class_name() const		{ return "RED"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int queue_size() const;
    const ewma_type &average_queue_size() const { return _size; }
    int drops() const				{ return _drops; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int check_params(unsigned min_thresh, unsigned max_thresh,
		     unsigned max_p, unsigned stability, ErrorHandler *errh) const;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void take_state(Element *e, ErrorHandler *errh);
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    bool should_drop();
    void handle_drop(Packet *);
    void push(int port, Packet *);
    Packet *pull(int port);

  protected:

    Storage *_queue1;
    Vector<Storage *> _queues;

    unsigned _min_thresh;
    unsigned _max_thresh;
    unsigned _kill_thresh;
    unsigned _max_p;		// out of 0xFFFF

    ewma_type _size;

    unsigned _C1;
    unsigned _C2;
    unsigned _G1;
    unsigned _G2;
    int _count;
    int _random_value;
    click_jiffies_t _last_jiffies;

    int _drops;
    Vector<Element *> _queue_elements;
    bool _gentle;

    void set_C1_and_C2();

    static String read_handler(Element *, void *) CLICK_COLD;

    int finish_configure(unsigned min_thresh, unsigned max_thresh, bool gentle,
			 unsigned max_p, unsigned stability,
			 const String &queues, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif

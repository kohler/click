// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_RED_HH
#define CLICK_RED_HH
#include <click/element.hh>
#include <click/ewma64.hh>
class Storage;

/*
=c

RED(MIN_THRESH, MAX_THRESH, MAX_P [, I<KEYWORDS>])

=s dropping

drops packets according to R<RED>

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

Keyword arguments are:

=over 8

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

=back

The RED element implements the Gentle R<RED> variant first proposed by Sally
Floyd in October 1997.

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
probability", October 1997. L<http://www.aciri.org/floyd/REDfunc.html>. */

class RED : public Element { public:

    RED();
    ~RED();

    const char *class_name() const		{ return "RED"; }
    const char *processing() const		{ return "a/ah"; }
    RED *clone() const;

    int queue_size() const;
    const DirectEWMA64 &average_queue_size() const { return _size; }
    int drops() const				{ return _drops; }

    void notify_noutputs(int);
    int configure(const Vector<String> &, ErrorHandler *);
    int check_params(unsigned, unsigned, unsigned, unsigned, ErrorHandler *) const;
    int initialize(ErrorHandler *);
    void take_state(Element *, ErrorHandler *);
    void configuration(Vector<String> &, bool *) const;
    bool can_live_reconfigure() const		{ return true; }
    int live_reconfigure(const Vector<String> &, ErrorHandler *);
    void add_handlers();

    bool should_drop();
    void handle_drop(Packet *);
    void push(int port, Packet *);
    Packet *pull(int port);

  protected:

    Storage *_queue1;
    Vector<Storage *> _queues;

    // Queue sizes are shifted by this much.
    static const unsigned QUEUE_SCALE = 10;

    unsigned _min_thresh;
    unsigned _max_thresh;
    unsigned _max_p;		// out of 0xFFFF

    DirectEWMA64 _size;

    unsigned _C1;
    unsigned _C2;
    unsigned _G1;
    unsigned _G2;
    int _count;
    int _random_value;
    int _last_jiffies;

    int _drops;
    Vector<Element *> _queue_elements;

    void set_C1_and_C2();

    static String read_stats(Element *, void *);
    static String read_queues(Element *, void *);
    static String read_parameter(Element *, void *);

    int finish_configure(unsigned min_thresh, unsigned max_thresh, unsigned max_p, unsigned stability, const String &queues, ErrorHandler *errh);
    
};

#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RATEDSPLITTER_HH
#define CLICK_RATEDSPLITTER_HH
#include <click/element.hh>
#include <click/tokenbucket.hh>
CLICK_DECLS

/*
 * =c
 * RatedSplitter(RATE, I[<KEYWORDS>])
 * =s shaping
 * splits flow of packets at specified rate
 * =processing
 * Push
 * =d
 *
 * RatedSplitter has two output ports. All incoming packets up to a maximum of
 * RATE packets per second are emitted on output port 0. Any remaining packets
 * are emitted on output port 1. Unlike Meter, RATE packets per second are
 * emitted on output port 0 even when the input rate is greater than RATE.
 *
 * Like RatedUnqueue, RatedSplitter is implemented using a token bucket that
 * defaults to a capacity of 20ms * RATE.  The capacity can be changed with the
 * BURST_DURATION and BURST_SIZE keyword configuration parameters.
 *
 * =e
 *   rs :: RatedSplitter(2000);
 * Split packets on port 0 at 2000 packets per second.
 *
 *   elementclass RatedSampler {
 *     input -> rs :: RatedSplitter(2000);
 *     rs [0] -> t :: Tee;
 *     t [0] -> [0] output;
 *     t [1] -> [1] output;
 *     rs [1] -> [0] output;
 *   };
 *
 * In the above example, RatedSampler is a compound element that samples input
 * packets at 2000 packets per second. All traffic is emitted on output 0; a
 * maximum of 2000 packets per second are emitted on output 1 as well.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item RATE
 *
 * Integer.  Token bucket fill rate in packets per second.
 *
 * =item BURST_DURATION
 *
 * Time.  If specified, the capacity of the token bucket is calculated as
 * rate * burst_duration.
 *
 * =item BURST_SIZE
 *
 * Integer.  If specified, the capacity of the token bucket is set to this
 * value.
 *
 * =h rate read/write
 * rate of splitting
 *
 * =a BandwidthRatedSplitter, ProbSplitter, Meter, Shaper, RatedUnqueue, Tee */

class RatedSplitter : public Element { public:

    RatedSplitter();

    const char *class_name() const	{ return "RatedSplitter"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PUSH; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers();

    void push(int port, Packet *);

 protected:

    TokenBucket _tb;

    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif

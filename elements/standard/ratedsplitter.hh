// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RATEDSPLITTER_HH
#define CLICK_RATEDSPLITTER_HH
#include <click/element.hh>
#include <click/gaprate.hh>
CLICK_DECLS

/*
 * =c
 * RatedSplitter(R)
 * =s shaping
 * splits flow of packets at specified rate
 * =d
 * 
 * RatedSplitter has two output ports. All incoming packets up to a maximum of
 * R packets per second are emitted on output port 0. Any remaining packets
 * are emitted on output port 1. Unlike Meter, R packets per second are
 * emitted on output port 0 even when the input rate is greater than R.
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
 * =h rate read/write
 * rate of splitting
 *
 * =a BandwidthRatedSplitter, ProbSplitter, Meter, Shaper, RatedUnqueue, Tee */

class RatedSplitter : public Element { public:
  
    RatedSplitter();
    ~RatedSplitter();

    const char *class_name() const	{ return "RatedSplitter"; }
    const char *port_count() const	{ return "1/2"; }
    const char *processing() const	{ return PUSH; }
    bool is_bandwidth() const		{ return class_name()[0] == 'B'; }
 
    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const	{ return true; }
    void configuration(Vector<String> &) const;
    void add_handlers();
  
    void push(int port, Packet *);

 protected:

    GapRate _rate;

};

CLICK_ENDDECLS
#endif

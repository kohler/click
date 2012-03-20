// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_RANDOMSAMPLE_HH
#define CLICK_RANDOMSAMPLE_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 *
 * RandomSample([P, I<KEYWORDS>])
 *
 * =s classification
 *
 * samples packets with some probability
 *
 * =d
 *
 * Samples packets with probability P. One out of 1/P packets are sent to the
 * first output. The remaining packets are dropped, unless the element has two
 * outputs, in which case they are emitted on output 1.
 *
 * If you don't specify P, you must supply one of the SAMPLE and DROP keyword
 * arguments.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item SAMPLE I<P>
 *
 * Sets the sampling probability to I<P>.
 *
 * =item DROP I<Q>
 *
 * The element will drop packets with probability I<Q>. Same as suppling (1 -
 * I<Q>) as the sampling probability.
 *
 * =item ACTIVE
 *
 * Boolean. RandomSample is active or inactive; when inactive, it sends all
 * packets to output 0. Default is true (active).
 *
 * =back
 *
 * =h sampling_prob read/write
 *
 * Returns or sets the sampling probability.
 *
 * =h drop_prob read/write
 *
 * Returns or sets the drop probability, which is 1 minus the sampling
 * probability.
 *
 * =h active read/write
 *
 * Makes the element active or inactive.
 *
 * =h drops read-only
 *
 * Returns the number of packets dropped.
 *
 * =a RandomBitErrors */

class RandomSample : public Element { public:

    RandomSample();

    const char *class_name() const		{ return "RandomSample"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers();

    void push(int port, Packet *);
    Packet *pull(int port);

  private:

    enum { SAMPLING_SHIFT = 28 };
    enum { SAMPLING_MASK = (1 << SAMPLING_SHIFT) - 1 };

    enum { h_sample, h_drop, h_config };

    uint32_t _sampling_prob;		// out of (1<<SAMPLING_SHIFT)
    bool _active;
    atomic_uint32_t _drops;

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif

// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_COUNTER_HH
#define CLICK_COUNTER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/llrpc.h>

/*
=c
Counter()

=s measurement
measures packet count and rate

=d

Passes packets unchanged from its input to its output, maintaining statistics
information about packet count and packet rate.

=h count read-only

Returns the number of packets that have passed through.

=h byte_count read-only

Returns the number of packets that have passed through.

=h rate read-only

Returns the recent arrival rate (measured by exponential
weighted moving average) in packets/bytes per second.

=h reset write-only

Resets the counts and rates to zero.

=h CLICK_LLRPC_GET_RATE llrpc

Argument is a pointer to an integer that must be 0.  Returns the recent
arrival rate (measured by exponential weighted moving average) in
packets per second. 

=h CLICK_LLRPC_GET_COUNT llrpc

Argument is a pointer to an integer that must be 0 (packet count) or 1 (byte
count). Returns the current packet or byte count.

=h CLICK_LLRPC_GET_COUNTS llrpc

Argument is a pointer to a click_llrpc_counts_st structure (see
<click/llrpc.h>). The C<keys> components must be 0 (packet count) or 1 (byte
count). Stores the corresponding counts in the corresponding C<values>
components.

*/

class Counter : public Element { public:

    Counter();
    ~Counter();

    const char *class_name() const		{ return "Counter"; }
    const char *processing() const		{ return AGNOSTIC; }

    void reset();

    Counter *clone() const			{ return new Counter; }
    int initialize(ErrorHandler *);
    void add_handlers();
    int llrpc(unsigned, void *);

    Packet *simple_action(Packet *);

  private:

#ifdef HAVE_INT64_TYPES
    uint64_t _count;
    uint64_t _byte_count;
#else
    uint32_t _count;
    uint32_t _byte_count;
#endif
    RateEWMA _rate;

    static String read_handler(Element *, void *);

};

#endif

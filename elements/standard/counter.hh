// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_COUNTER_HH
#define CLICK_COUNTER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/llrpc.h>
class HandlerCall;

/*
=c

Counter([I<KEYWORDS>])

=s measurement

measures packet count and rate

=d

Passes packets unchanged from its input to its output, maintaining statistics
information about packet count and packet rate.

Keyword arguments are:

=over 8

=item CALL_AFTER_COUNT

Argument is `I<N> I<HANDLER> [I<VALUE>]'. When the packet count reaches I<N>,
call the write handler I<HANDLER> with value I<VALUE>.

=item CALL_AFTER_BYTES

Argument is `I<N> I<HANDLER> [I<VALUE>]'. When the byte count reaches or
exceeds I<N>, call the write handler I<HANDLER> with value I<VALUE>.

=back

=h count read-only

Returns the number of packets that have passed through.

=h byte_count read-only

Returns the number of packets that have passed through.

=h rate read-only

Returns the recent arrival rate (measured by exponential
weighted moving average) in packets/bytes per second.

=h reset write-only

Resets the counts and rates to zero.

=h call_after_count write-only

Writes a new CALL_AFTER_COUNT argument. The handler can be omitted.

=h call_after_bytes write-only

Writes a new CALL_AFTER_BYTES argument. The handler can be omitted.

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
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();
    int llrpc(unsigned, void *);

    Packet *simple_action(Packet *);

  private:

#ifdef HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    
    counter_t _count;
    counter_t _byte_count;
    RateEWMA _rate;

    counter_t _count_trigger;
    HandlerCall *_count_trigger_h;

    counter_t _byte_trigger;
    HandlerCall *_byte_trigger_h;

    bool _count_triggered : 1;
    bool _byte_triggered : 1;

    static String read_handler(Element *, void *);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);

};

#endif

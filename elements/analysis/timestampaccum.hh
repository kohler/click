// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TIMESTAMPACCUM_HH
#define CLICK_TIMESTAMPACCUM_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TimestampAccum()

=s measurement

collects differences in timestamps

=d

For each passing packet, measures the elapsed time since the packet's
timestamp. Keeps track of the total elapsed time accumulated over all packets.

=n

=h count read-only
Returns the number of packets that have passed.

=h time read-only
Returns the accumulated timestamp difference for all passing packets.

=h average_time read-only
Returns the average timestamp difference over all passing packets.

=h reset_counts write-only
Resets C<count> and C<time> counters to zero when written.

=a SetCycleCount, RoundTripCycleCount, SetPerfCount, PerfCountAccum */

class TimestampAccum : public Element { public:
    
    TimestampAccum();
    ~TimestampAccum();
  
    const char *class_name() const	{ return "TimestampAccum"; }
    const char *processing() const	{ return AGNOSTIC; }
    TimestampAccum *clone() const	{ return new TimestampAccum; }

    int initialize(ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:
  
    double _usec_accum;
    uint64_t _count;

    static String read_handler(Element *, void *);
    static int reset_handler(const String &, Element *, void *, ErrorHandler *);
  
};

CLICK_ENDDECLS
#endif

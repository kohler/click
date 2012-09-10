// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TIMERANGE_HH
#define CLICK_TIMERANGE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TimeRange(I<keyword SIMPLE>)

=s timestamps

monitor range of packet timestamps

=d

TimeRange passes packets along unchanged, monitoring the smallest range that
contains all of their timestamps. You can access that range with handlers.

Keyword arguments are:

=over 8

=item SIMPLE

Boolean. If true, then packets arrive at TimeRange with monotonically
increasing timestamps. Default is false.

=back

=h first read-only

Returns the earliest timestamp observed, or "0.0" if no packets have passed.

=h last read-only

Returns the latest timestamp observed, or "0.0" if no packets have passed.

=h range read-only

Returns the earliest and latest timestamps observed, separated by a space.

=h interval read-only

Returns the difference between the earliest and latest timestamps observed,
in seconds.

=h reset write-only

Clears the stored range. Future packets will accumulate a new range.

=a

TimeFilter */

class TimeRange : public Element { public:

    TimeRange() CLICK_COLD;
    ~TimeRange() CLICK_COLD;

    const char *class_name() const	{ return "TimeRange"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    Timestamp _first;
    Timestamp _last;

    bool _simple;

    enum {
	h_range, h_interval, h_reset
    };
    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler*) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

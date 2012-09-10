// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEPAINT_HH
#define CLICK_AGGREGATEPAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AggregatePaint([BITS, I<KEYWORDS>])

=s aggregates

sets aggregate annotation based on paint annotation

=d

AggregatePaint sets the aggregate annotation on every passing packet to a
portion of that packet's paint annotation.  By default, it is simply set to
the paint annotation; but if you specify a number for BITS, then only the
low-order BITS bits of the annotation are used.

Keyword arguments are:

=over 8

=item INCREMENTAL

Boolean.  If true, then incrementally update the aggregate annotation: given a
paint annotation with value V, and an old aggregate annotation of O, the new
aggregate annotation will equal (O * 2^LENGTH) + V.  Default is false.

=back

=a

AggregateLength, AggregateIPFlows, AggregateCounter, AggregateIP

*/

class AggregatePaint : public Element { public:

    AggregatePaint() CLICK_COLD;
    ~AggregatePaint() CLICK_COLD;

    const char *class_name() const	{ return "AggregatePaint"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    int _bits;
    bool _incremental;

};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ADJUSTTIMESTAMP_HH
#define CLICK_ADJUSTTIMESTAMP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AdjustTimestamp(TIME [, I<keywords> FIRST, ALL])

=s timestamps

adjust the time in the packet's timestamp annotation

=d

Adjust the packet's timestamp annotation by adding TIME (a possibly-negative
time value).  Zero-valued timestamps are not changed by default.

Keyword arguments are:

=over 8

=item FIRST

Boolean.  If true, then adjust the packet's "first timestamp" annotation, not
its timestamp annotation.  Default is false.

=item ALL

Boolean.  If true, adjust zero-valued timestamps as well as other timestamps.
Default is false.

=back

=h time read/write

Return or set the TIME argument.

=a SetTimestamp, SetTimestampDelta */

class AdjustTimestamp : public Element { public:

    AdjustTimestamp() CLICK_COLD;
    ~AdjustTimestamp() CLICK_COLD;

    const char *class_name() const		{ return "AdjustTimestamp"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    bool _first;
    bool _all;
    Timestamp _ts;

};

CLICK_ENDDECLS
#endif

// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_SETTIMESTAMPDELTA_HH
#define CLICK_SETTIMESTAMPDELTA_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetTimestampDelta([I<keywords> TYPE])

=s timestamps

replace packet timestamps with deltas

=d

SetTimestampDelta replaces nonzero packet timestamps with deltas.

Keyword arguments are:

=over 8

=item TYPE

Sets the type of delta.  The default is C<RANGE>, which means the delta
relative to the first nonzero packet timestamp encountered.  Other
possibilities are C<NOW>, which means the delta between its current timestamp
and now, and C<FIRST>, which means the delta between its first timestamp
annotation and its current timestamp.

=back

=h first read-only

Returns the earliest nonzero timestamp observed, if any.

=h reset write-only

Clears the timestamp record. Future packet timestamps will be measured
relative to the next nonzero timestamp encountered.

=a

SetTimestamp, AdjustTimestamp, TimeFilter */

class SetTimestampDelta : public Element { public:

    SetTimestampDelta() CLICK_COLD;
    ~SetTimestampDelta() CLICK_COLD;

    const char *class_name() const	{ return "SetTimestampDelta"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    int _type;
    Timestamp _first;

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler*) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

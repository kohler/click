// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_SETTIMESTAMPDELTA_HH
#define CLICK_SETTIMESTAMPDELTA_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetTimestampDelta()

=s measurement, analysis

replace packet timestamps with deltas

=d

SetTimestampDelta passes packets through, but nonzero packet timestamps are
replaced with deltas relative to the first nonzero timestamp encountered.

Keyword arguments are:

=h first read-only

Returns the earliest nonzero timestamp observed, if any.

=h reset write-only

Clears the timestamp record. Future packet timestamps will be measured
relative to the next nonzero timestamp encountered.

=a

TimeFilter */

class SetTimestampDelta : public Element { public:

    SetTimestampDelta();
    ~SetTimestampDelta();

    const char *class_name() const	{ return "SetTimestampDelta"; }
    const char *processing() const	{ return AGNOSTIC; }

    void add_handlers();

    Packet *simple_action(Packet *);
    
  private:

    struct timeval _first;

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler*);
    
};

CLICK_ENDDECLS
#endif

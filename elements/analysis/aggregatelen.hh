// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATELEN_HH
#define CLICK_AGGREGATELEN_HH
#include <click/element.hh>

/*
=c

AggregateLength(I<KEYWORDS>)

=s

sets aggregate annotation based on packet length

=d

AggregateLength sets the aggregate annotation on every passing packet to the
packet's length, plus any length stored in the extra length annotation.

Keyword arguments are:

=over 8

=item IP

Boolean. If true, then only include length starting at the IP header. Default
is false.

=back

=n

If IP is true, then packets without a network header are pushed onto output 1,
or dropped if there is no output 1.

=a

AggregateIP, AggregateCounter

*/

class AggregateLength : public Element { public:

    AggregateLength();
    ~AggregateLength();

    const char *class_name() const	{ return "AggregateLength"; }
    AggregateLength *clone() const	{ return new AggregateLength; }

    const char *processing() const	{ return AGNOSTIC; }
    int configure(Vector<String> &, ErrorHandler *);

    void push(int, Packet *);
    Packet *pull(int);
    
  private:

    bool _ip;

    Packet *handle_packet(Packet *);
    Packet *bad_packet(Packet *);
    
};

#endif

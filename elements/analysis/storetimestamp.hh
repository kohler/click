// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_STORETIMESTAMP_HH
#define CLICK_STORETIMESTAMP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

StoreTimestamp([I<keywords> OFFSET, TAIL])

=s timestamps

store packet timestamps in packet data

=d

StoreTimestamp stores each packet's timestamp annotation somewhere in its
data.  Supply exactly one of the OFFSET and TAIL keywords.

Keyword arguments are:

=over 8

=item OFFSET

Integer.  Store the timestamp starting OFFSET bytes into the packet.  The
packet will be extended, if necessary, so that it's at least OFFSET+8 bytes
long.

=item TAIL

Boolean.  If true, the timestamp will be tacked on to the end of the packet.

=back

=a

SetTimestamp */

class StoreTimestamp : public Element { public:

    StoreTimestamp() CLICK_COLD;
    ~StoreTimestamp() CLICK_COLD;

    const char *class_name() const	{ return "StoreTimestamp"; }
    const char *port_count() const	{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    int _offset;

};

CLICK_ENDDECLS
#endif

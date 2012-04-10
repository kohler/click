// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PACKETTEST_HH
#define CLICK_PACKETTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

PacketTest()

=s test

runs regression tests for Packet

=d

PacketTest runs Packet regression tests at initialization time. It does not
route packets.

=a

CheckPacket */

class PacketTest : public Element { public:

    PacketTest();

    const char *class_name() const		{ return "PacketTest"; }

    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

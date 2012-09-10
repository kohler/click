// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_SETPACKETTYPE_HH
#define CLICK_SETPACKETTYPE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetPacketType(TYPE)

=s annotations

sets packet type annotation

=d

SetPacketType sets passing packets' packet type annotations to TYPE. The
packet type annotation tells Linux about the packet's link-level
characteristics. For example, was the packet sent directly to this host, or
was it broadcast? TYPE should be one of `C<HOST>', `C<BROADCAST>',
`C<MULTICAST>', `C<OTHERHOST>', `C<OUTGOING>', or `C<LOOPBACK>'. */

class SetPacketType : public Element { public:

    SetPacketType() CLICK_COLD;

    const char *class_name() const		{ return "SetPacketType"; }
    const char *port_count() const		{ return PORTS_1_1; }

    static int parse_type(const String &);
    static const char *unparse_type(int);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    Packet *simple_action(Packet *);

  protected:

    Packet::PacketType _ptype;

};

CLICK_ENDDECLS
#endif

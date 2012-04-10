// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CHECKPACKET_HH
#define CLICK_CHECKPACKET_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CheckPacket(I<KEYWORDS>)

=s test

check packets against a specification

=d

CheckPacket compares all received packets against a specification provided by
keyword arguments. It prints error messages when incoming packets don't match
the spec.

Keyword arguments are as follows. Tests are performed for the keyword
arguments you specify. If you don't want to run a test, don't supply the
keyword. CheckPacket(), with no keywords, accepts every packet.

=over 8

=item DATA

String. The contents of the packet (starting DATA_OFFSET bytes in) must
exactly match DATA.

=item DATA_OFFSET

Integer. Specifies the offset into the packet used for DATA matches. Default
is 0.

=item LENGTH

Integer. The packet's length must equal LENGTH.

=item LENGTH_GE

Integer. The packet's length must be at least LENGTH_GE.

=item LENGTH_LE

Integer. The packet's length must be at most LENGTH_LE. Specify at most one of
LENGTH, LENGTH_GE, and LENGTH_LE.

=item ALIGNMENT

Two space-separated integers, `MODULUS OFFSET'. The packet's data must be
aligned OFFSET bytes off from a MODULUS-byte boundary.

=a

PacketTest */

class CheckPacket : public Element { public:

    CheckPacket();

    const char *class_name() const		{ return "CheckPacket"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);

  private:

    String _data;
    uint32_t _data_offset;
    int _alignment_chunk;
    int _alignment_offset;
    uint32_t _length;
    uint8_t _data_op;
    uint8_t _length_op;
    bool _do_align : 1;

};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CLIPBOARD_HH
#define CLICK_CLIPBOARD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
Clipboard(RANGE_1, ..., RANGE_N)

=s basicmod
copies data from one packet to another

=d
Takes as arguments one or more byte ranges to copy. When Clipboard receives a
packet on input 0, it copies the selected bytes to the clipboard buffer and
emits the packet unchanged on output 0. When receiving a packet on input 1,
Clipboard updates the packet with the bytes from the buffer and outputs the
modified packet on output 1. This way, data from a single packet can be copied
to 0 or more packets.

Each RANGE is on the form C<x/n>, where C<x> is the offset and C<n> the number
of bytes.

Each Clipboard input/output pair can work in either a push or a pull context.

Clipboard can also be used to copy data from one place in a packet to another,
by looping the same packet back through Clipboard and using Strip and Unstrip
to offset the packet data.

Passing a packet through input/output 1 before any packets have passed through
input/output 0 will cause undefined data to be written into the packet. Passing
a packet through input/output 1 which is too small for any one of the ranges
will cause the packet contents to be undefined.

=a StoreData, Strip, Unstrip
*/


struct Range {
    uint32_t offset;
    uint32_t length;
};


class Clipboard : public Element {
public:
    Clipboard() CLICK_COLD;

    const char *class_name() const { return "Clipboard"; }
    const char *flags()      const { return "S0"; }
    const char *flow_code()  const { return "#/#"; }
    const char *port_count() const { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const { return true; }

    Packet *pull(int port);
    void push(int port, Packet *p);

private:
    Vector<unsigned char> _clipboard;
    Vector<Range> _ranges;
    uint32_t _minPacketLength;

    void copy(Packet *p);
    Packet *paste(Packet *p);
};


CLICK_ENDDECLS
#endif

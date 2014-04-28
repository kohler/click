// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_RESIZE_HH
#define CLICK_RESIZE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
Resize(HEAD, TAIL)

=s basicmod
adjust packet length.

=d
Adds HEAD bytes in front of packet and TAIL bytes at the end of the packet.
If either number is negative, bytes are removed instead of added.

Any added bytes are left uninitialized, and no copy of the packet is made if
the added bytes fit within the packet's existing headroom and/or tailroom.

=a Strip, Unstrip, Truncate
*/


class Resize : public Element {
public:
    Resize() CLICK_COLD;

    const char *class_name() const { return "Resize"; }
    const char *port_count() const { return PORTS_1_1; }

    // This element neither generates nor consumes packets.
    const char *flags()      const { return "S0"; }

    void add_handlers() CLICK_COLD;
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const { return true; }

    Packet *simple_action(Packet *);

private:
    int _head;
    int _tail;
};


CLICK_ENDDECLS
#endif

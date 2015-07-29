// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAD_HH
#define CLICK_PAD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Pad([LENGTH, I<keyword> ZERO])

=s basicmod

extend packet length

=d

Extend packets to at least LENGTH bytes.

If LENGTH is omitted, then input packets are extended to the length
indicated by their extra length annotations. Output packets always have
extra length annotation zero.

Keyword arguments are:

=over 8

=item ZERO

Boolean. If true, then set added packet data to zero; if false, then
additional packet data is left uninitialized (which might be a security
problem). Default is true.

=back

=a Truncate
*/

class Pad : public Element { public:

    Pad() CLICK_COLD;

    const char *class_name() const		{ return "Pad"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    Packet *simple_action(Packet *);

  private:

    unsigned _nbytes;
    bool _zero;

};

CLICK_ENDDECLS
#endif

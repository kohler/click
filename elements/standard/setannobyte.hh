#ifndef CLICK_SETANNOBYTE_HH
#define CLICK_SETANNOBYTE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetAnnoByte(ANNO, VALUE)

=s annotations

sets packet user annotations

=d

Sets each packet's user annotation byte at ANNO to VALUE, an integer
0..255.  Permissible values for ANNO are 0 to n-1, inclusive, where
n is typically 48, or the name of a one-byte annotation.

=h anno read-only
Returns ANNO as an integer offset
=h value read/write
Returns or sets VALUE

=a Paint */

class SetAnnoByte : public Element { public:

    SetAnnoByte() CLICK_COLD;

    const char *class_name() const		{ return "SetAnnoByte"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    int _offset;
    unsigned char _value;

};

CLICK_ENDDECLS
#endif

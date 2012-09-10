#ifndef CLICK_VLANDECAP_HH
#define CLICK_VLANDECAP_HH
#include <click/element.hh>

CLICK_DECLS

/*
=c

VLANDecap([ANNO])

=s ethernet

strip VLAN information from Ethernet packets

=d

Expects a potentially 802.1Q VLAN encapsulated packet as input.  If it is
encapsulated, then the encapsulation is stripped, leaving a conventional
Ethernet packet.

If ANNO is true (the default), then the VLAN_TCI annotation is set to the VLAN
TCI in network byte order, or 0 if the packet was not VLAN-encapsulated.

=a

VLANEncap
*/

class VLANDecap : public Element { public:

    VLANDecap() CLICK_COLD;
    ~VLANDecap() CLICK_COLD;

    const char *class_name() const	{ return "VLANDecap"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    Packet *simple_action(Packet *);

private:

    bool _anno;

};

CLICK_ENDDECLS
#endif

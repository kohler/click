#ifndef CLICK_VLANDECAP_HH
#define CLICK_VLANDECAP_HH
#include <click/element.hh>

CLICK_DECLS

/*
=c

VLANDecap(I<keywords>])

=s ethernet

strip VLAN information from Ethernet packets

=d

Expects a potentially 802.1Q VLAN encapsulated packet as input.  If it is
encapsulated, then the encapsulation is stripped, leaving a conventional
Ethernet packet.

Keyword arguments are:

=item ANNO

If ANNO is true (the default), then the VLAN_TCI annotation is set to the VLAN
TCI in network byte order, or 0 if the packet was not VLAN-encapsulated.

=item ETHERTYPE

Specifies the ethertype designating VLAN encapsulated packets. The default is
0x8100 (standard 802.1Q customer VLANs); other useful values are 0x88a8 (for
802.1ad service VLANs, aka QinQ) and 0x9100 (old non-standard VLANs).

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
    uint16_t _ethertype;

};

CLICK_ENDDECLS
#endif

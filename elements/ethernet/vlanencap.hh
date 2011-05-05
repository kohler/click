#ifndef CLICK_VLANENCAP_HH
#define CLICK_VLANENCAP_HH
#include <click/element.hh>

CLICK_DECLS

/*
=c

VLANEncap(TCI_ANNO, I<keywords>))

=s ethernet

Expects ethernet-encapsulated packets as input.  If TCI_ANNO is
non-zero, adds an 802.1q header with network-byte-order bits from
TCI_ANNO in it.

Keyword arguments are:

=over 8

=item VID

VID is the 12-bit VLAN id to use in the packet

=back

=a

VLANDecap
 */

class VLANEncap : public Element { public:

    VLANEncap();
    ~VLANEncap();

    const char *class_name() const	{ return "VLANEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return AGNOSTIC; }
    void add_handlers();

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);

private:

    uint8_t _tci_anno;
    uint16_t _vid;
};

CLICK_ENDDECLS
#endif

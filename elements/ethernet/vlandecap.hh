#ifndef CLICK_VLANDECAP_HH
#define CLICK_VLANDECAP_HH
#include <click/element.hh>

CLICK_DECLS

/*
=c

VLANDecap(WRITE_TCI_ANNO)

=s ethernet

Expects a potentially VLAN encapsulated packet as input.  If it is
encapsulated, then the encapsulation is stripped and WRITE_TCI_ANNO is
set to the encapsulated value in network-byte-order, otherwise
WRITE_TCI_ANNO is set to 0.

=d

=a

VLANEncap
*/

class VLANDecap : public Element { public:

    VLANDecap();
    ~VLANDecap();

    const char *class_name() const	{ return "VLANDecap"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    Packet *simple_action(Packet *);

private:

    int _tci_anno;
};

CLICK_ENDDECLS
#endif

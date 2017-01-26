#ifndef CLICK_VLANENCAP_HH
#define CLICK_VLANENCAP_HH
#include <click/element.hh>

CLICK_DECLS

/*
=c

VLANEncap([VLAN_TCI, VLAN_PCP, <keywords>])

=s ethernet

add VLAN information to Ethernet packets

=d

Expects Ethernet-encapsulated packets as input.  Adds an 802.1Q shim header.
The resulting packet looks like an Ethernet packet with type 0x8100.  The VLAN
arguments define the VLAN ID and priority code point.

VLAN_TCI is a 16-bit VLAN TCI, including both the VLAN ID and the VLAN PCP.
VLAN_TCI may also be the string "ANNO"; in ANNO mode, the encapsulated VLAN
TCI is read from the input packet's VLAN_TCI annotation.  You may also set the
VLAN_ID and VLAN_PCP separately via keywords.

If you want to add a full Ethernet 802.1Q header, use EtherVLANEncap.

Keyword arguments are:

=item VLAN_ID

The VLAN ID, a number between 0 and 0xFFF.  Exactly one of VLAN_ID and
VLAN_TCI must be set.  Note that VLAN IDs 0 and 0xFFF are reserved in 802.1Q.

=item VLAN_PCP

The VLAN Priority Code Point, a number between 0 and 7.  Defaults to 0.

=item NATIVE_VLAN

The native VLAN, a number between -1 and 0xFFF.  If the output VLAN ID equals
NATIVE_VLAN, then the output packet is encapsulated in a conventional Ethernet
header, rather than an 802.1Q header (i.e., the shim header is not added).
Set to -1 for no native VLAN.  Defaults to 0.

=item ETHERTYPE

Specifies the ethertype designating VLAN encapsulated packets. The default is
0x8100 (standard 802.1Q customer VLANs); other useful values are 0x88a8 (for
802.1ad service VLANs, aka QinQ) and 0x9100 (old non-standard VLANs).

=a

EtherVLANEncap, VLANDecap
*/

class VLANEncap : public Element { public:

    VLANEncap() CLICK_COLD;
    ~VLANEncap() CLICK_COLD;

    const char *class_name() const	{ return "VLANEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }
    void add_handlers() CLICK_COLD;

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    uint16_t _vlan_tci;
    bool _use_anno;
    int _native_vlan;
    uint16_t _ethertype;

    enum { h_config, h_vlan_tci };
    static String read_handler(Element *e, void *user_data) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

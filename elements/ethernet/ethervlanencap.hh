#ifndef CLICK_ETHERVLANENCAP_HH
#define CLICK_ETHERVLANENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EtherVLANEncap(ETHERTYPE, SRC, DST, VLAN_TCI [, VLAN_PCP, I<keywords>])

=s ethernet

encapsulates packets in 802.1Q VLAN Ethernet header

=d

Encapsulates each packet in the 802.1Q header specified by its arguments.  The
resulting packet looks like an Ethernet packet with type 0x8100.  The
encapsulated Ethernet type is ETHERTYPE, which should be in host order.  The
VLAN arguments define the VLAN ID and priority code point.

VLAN_TCI is a 16-bit VLAN TCI, including both the VLAN ID and the VLAN PCP.
VLAN_TCI may also be the string "ANNO"; in ANNO mode, the encapsulated VLAN
TCI is read from the input packet's VLAN_TCI annotation.  You may also set the
VLAN_ID and VLAN_PCP separately via keywords.

If you want to add a VLAN shim header to a packet that's already
Ethernet-encapsulated, use VLANEncap.

Keyword arguments are:

=item VLAN_ID

The VLAN ID, a number between 0 and 0xFFF.  Exactly one of VLAN_ID and
VLAN_TCI must be set.  Note that VLAN IDs 0 and 0xFFF are reserved in 802.1Q.

=item VLAN_PCP

The VLAN Priority Code Point, a number between 0 and 7.  Defaults to 0.

=item NATIVE_VLAN

The native VLAN, a number between -1 and 0xFFF.  If the output VLAN ID equals
NATIVE_VLAN, then the output packet is encapsulated in a conventional Ethernet
header, rather than an 802.1Q header.  Set to -1 for no native VLAN.  Defaults
to 0.

=e

Encapsulate packets in an 802.1Q Ethernet VLAN header with type ETHERTYPE_IP
(0x0800), source address 1:1:1:1:1:1, destination address 2:2:2:2:2:2, and
VLAN ID 1:

  EtherVLANEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2, 1)

=h src read/write

Return or set the SRC parameter.

=h dst read/write

Return or set the DST parameter.

=h ethertype read/write

Return or set the ETHERTYPE parameter.

=h vlan_tci read/write

Return or set the VLAN_TCI parameter.

=h vlan_id read/write

Return or set the VLAN_ID parameter.

=h vlan_pcp read/write

Return or set the VLAN_PCP parameter.

=h native_vlan read/write

Return or set the NATIVE_VLAN parameter.

=a

VLANEncap, StripEtherVLANHeader, SetVLANAnno, EtherEncap, ARPQuerier,
EnsureEther, StoreEtherAddress */

class EtherVLANEncap : public Element { public:

    EtherVLANEncap() CLICK_COLD;
    ~EtherVLANEncap() CLICK_COLD;

    const char *class_name() const	{ return "EtherVLANEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:

    click_ether_vlan _ethh;
    bool _use_anno;
    int _native_vlan;

    enum { h_config, h_vlan_tci };
    static String read_handler(Element *e, void *user_data) CLICK_COLD;
    static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

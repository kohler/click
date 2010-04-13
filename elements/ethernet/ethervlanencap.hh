#ifndef CLICK_ETHERVLANENCAP_HH
#define CLICK_ETHERVLANENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EtherVlanEncap(ETHERTYPE, SRC, DST, VLAN [, VLAN_PCP, I<keywords> NATIVE_VLAN])

=s ethernet

encapsulates packets in 802.1Q VLAN Ethernet header

=d

Encapsulates each packet in the 802.1Q header specified by its arguments.  The
resulting packet looks like an Ethernet packet with type 0x8100.  The
encapsulated Ethernet type is ETHERTYPE, which should be in host order.  The
VLAN and VLAN_PCP arguments define the VLAN ID and priority code point.

VLAN must be between 0 and 0xFFE.  It may also be the string "ANNO".  In VLAN
ANNO mode, the encapsulated VLAN and VLAN_PCP are read from the input packet's
VLAN annotation.  Also, if the NATIVE_VLAN keyword is set, and the VLAN
annotation equals NATIVE_VLAN, then the output packet is encapsulated in a
conventional Ethernet header, rather than an 802.1Q header.

VLAN_PCP defaults to 0, and must be between 0 and 7.

=e

Encapsulate packets in an 802.1Q Ethernet VLAN header with type ETHERTYPE_IP
(0x0800), source address 1:1:1:1:1:1, destination address 2:2:2:2:2:2, and
VLAN ID 1:

  EtherVlanEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2, 1)

=h src read/write

Return or set the SRC parameter.

=h dst read/write

Return or set the DST parameter.

=h ethertype read/write

Return or set the ETHERTYPE parameter.

=h vlan read/write

Return or set the VLAN parameter.

=h vlan_pcp read/write

Return or set the VLAN_PCP parameter.

=a

StripEtherVlanHeader, SetVlanAnno, EtherEncap, ARPQuerier, EnsureEther,
StoreEtherAddress */

class EtherVlanEncap : public Element { public:

    EtherVlanEncap();
    ~EtherVlanEncap();

    const char *class_name() const	{ return "EtherVlanEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers();

    Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:

    click_ether_vlan _ethh;
    bool _use_anno;
    bool _use_native_vlan;
    uint16_t _native_vlan;

    enum { h_vlan, h_vlan_pcp };
    static String read_handler(Element *e, void *user_data);

};

CLICK_ENDDECLS
#endif

#ifndef CLICK_STOREIP6ADDRESS_HH
#define CLICK_STOREIP6ADDRESS_HH
#include <click/element.hh>
#include <click/ip6address.hh>
CLICK_DECLS

/*
=c
StoreIP6Address(OFFSET)
StoreIP6Address(ADDR, OFFSET)
=s ip6
stores IPv6 address in packet
=d

The one-argument form writes the destination IPv6 address annotation into the
packet at offset OFFSET, usually an integer. But if the annotation is zero, it
doesn't change the packet.

The two-argument form writes IPv6 address ADDR into the packet at offset
OFFSET.

The OFFSET argument may also be a special thing as 'src' or 'dst' refering to respectively, 
the offset of the source address and destination address in an IPv6 packet.

=e
FromDevice(eth1)
-> Strip(14)
-> CheckIP6Header
-> ip6filter :: IP6Classifier(src host 2001:2:f000::1 and nxt 59)

ip6filter[0]
-> DecIP6HLIM
-> Unstrip(14)
-> StoreEtherAddress(00:0a:95:9d:68:16, 'src')
-> StoreEtherAddress(00:0a:95:9d:68:17, 'dst')
-> StoreIP6Address(2001:2:f000::3, 'src')   // change source address to 2001:2:f000::3
-> StoreIP6Address(2001:2:1::4, 'dst')      // change destination address to 2001:2:1::4
-> q :: Queue
-> ToDevice(eth0)

ip6filter[1]
-> Discard;

=a
CheckIP6Header
*/

class StoreIP6Address : public Element { public:

    StoreIP6Address() CLICK_COLD;
    ~StoreIP6Address() CLICK_COLD;

    const char *class_name() const		{ return "StoreIP6Address"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:
    int _offset;
    IP6Address _address;
    bool _address_given;
};

CLICK_ENDDECLS
#endif

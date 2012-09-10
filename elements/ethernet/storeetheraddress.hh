#ifndef CLICK_STOREETHERADDRESS_HH
#define CLICK_STOREETHERADDRESS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

StoreEtherAddress(ADDR, OFFSET)

=s ethernet

stores Ethernet address in packet

=d

Writes an Ethernet address ADDR into the packet at offset OFFSET.  If OFFSET
is out of range, the input packet is dropped or emitted on optional output 1.

The OFFSET argument may be 'src' or 'dst'.  These strings are equivalent to
offsets 6 and 0, respectively, which are the offsets into an Ethernet header
of the source and destination Ethernet addresses.

=h addr read/write

Return or set the ADDR argument.

=a

EtherEncap
*/

class StoreEtherAddress : public Element { public:

    StoreEtherAddress() CLICK_COLD;
    ~StoreEtherAddress() CLICK_COLD;

    const char *class_name() const		{ return "StoreEtherAddress"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

 private:

    unsigned _offset;
    EtherAddress _address;

};

CLICK_ENDDECLS
#endif

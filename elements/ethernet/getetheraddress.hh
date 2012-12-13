#ifndef CLICK_GETETHERADDRESS_HH
#define CLICK_GETETHERADDRESS_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

GetEtherAddress(ANNO, OFFSET)

=s ethernet

stores an ethernet address from a packet into an annotation.

=d

Writes an Ethernet address ADDR into the packet at offset OFFSET.  If OFFSET
is out of range, the input packet is dropped or emitted on optional output 1.

The OFFSET argument may be 'src' or 'dst'.  These strings are equivalent to
offsets 6 and 0, respectively, which are the offsets into an Ethernet header
of the source and destination Ethernet addresses.

=a

EtherEncap, SetEtherAddress, StoreEtherAddress
*/

class GetEtherAddress : public Element {

  public:

    const char *class_name() const		{ return "GetEtherAddress"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    Packet *simple_action(Packet *);

  private:
    uint32_t _offset;
    int _anno;

};

CLICK_ENDDECLS
#endif

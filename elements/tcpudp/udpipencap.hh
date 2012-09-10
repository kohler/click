#ifndef CLICK_UDPIPENCAP_HH
#define CLICK_UDPIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/udp.h>
CLICK_DECLS

/*
=c

UDPIPEncap(SRC, SPORT, DST, DPORT [, CHECKSUM])

=s udp

encapsulates packets in static UDP/IP headers

=d

Encapsulates each incoming packet in a UDP/IP packet with source address
SRC, source port SPORT, destination address DST, and destination port
DPORT. The UDP checksum is calculated if CHECKSUM? is true; it is true by
default.

As a special case, if DST is "DST_ANNO", then the destination address
is set to the incoming packet's destination address annotation.

The UDPIPEncap element adds both a UDP header and an IP header.

The Strip element can be used by the receiver to get rid of the
encapsulation header.

=e
  UDPIPEncap(1.0.0.1, 1234, 2.0.0.2, 1234)

=h src read/write

Returns or sets the SRC source address argument.

=h sport read/write

Returns or sets the SPORT source port argument.

=h dst read/write

Returns or sets the DST destination address argument.

=h dport read/write

Returns or sets the DPORT destination port argument.

=a Strip, IPEncap
*/

class UDPIPEncap : public Element { public:

    UDPIPEncap() CLICK_COLD;
    ~UDPIPEncap() CLICK_COLD;

    const char *class_name() const	{ return "UDPIPEncap"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *flags() const		{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    struct in_addr _saddr;
    struct in_addr _daddr;
    uint16_t _sport;
    uint16_t _dport;
    bool _cksum;
    bool _use_dst_anno;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
    bool _checked_aligned;
#endif
    atomic_uint32_t _id;

    static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

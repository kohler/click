#ifndef CLICK_UDPIP6ENCAP_HH
#define CLICK_UDPIP6ENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/udp.h>
#include <clicknet/ip6.h>
#include <click/ip6address.hh>
CLICK_DECLS

/*
=c

UDPIP6Encap(SRC, SPORT, DST, DPORT)

=s udp

encapsulates packets in static UDP/IP6 headers

=d

Encapsulates each incoming packet in a UDP/IP6 packet with source address
SRC, source port SPORT, destination address DST, and destination port
DPORT. The UDP checksum is always calculated, since in IPv6 the UDP checksum is mandatory.

As a special case, if DST is "DST_ANNO", then the destination address
is set to the incoming packet's destination address annotation.

The UDPIP6Encap element adds both a UDP header and an IP6 header.

The Strip element can be used by the receiver to get rid of the
encapsulation header.

=e
  UDPIPEncap(2001:2001:2001:2001::1, 1234, 2001:2001:2001:2001::2, 1234)

=h src read/write

Returns or sets the SRC source address argument.

=h sport read/write

Returns or sets the SPORT source port argument.

=h dst read/write

Returns or sets the DST destination address argument.

=h dport read/write

Returns or sets the DPORT destination port argument.

=a Strip
*/

class UDPIP6Encap : public Element { public:

    UDPIP6Encap();
    ~UDPIP6Encap();

    const char *class_name() const	{ return "UDPIP6Encap"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *flags() const		{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    struct click_in6_addr _saddr;
    struct click_in6_addr _daddr;
    uint16_t _sport;
    uint16_t _dport;
    bool _use_dst_anno;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
    bool _checked_aligned;
#endif

    static String read_handler(Element *, void *) CLICK_COLD;


};

CLICK_ENDDECLS
#endif

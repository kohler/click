#ifndef CLICK_ROUTING0ENCAP_HH
#define CLICK_ROUTING0ENCAP_HH
#include <click/element.hh>
#include <click/ip6address.hh>

#include <click/vector.hh>
CLICK_DECLS

/*
 * =c
 * RoutingZeroEncap(PROTO ADDRESSES)
 * =s ip6
 *
 * =d
 *
 * Encapsulates a packet in an IPv6 Routing Extension Header with
 * as option type 0.
 *
 * Proto is the protocol following the RoutingZeroEncap header.
 * ADDRESSES is a space separated list of ipv6 addresses to be visited
 * before arriving the final node. Note that the list must be given
 * between "" (double quotes), see below for an example.
 *
 * =e
 *
 * InfiniteSource(LIMIT 1)
 *   -> UDPEncap(1200,1500)
 *   -> RoutingZeroEncap(PROTO 17, ADDRESSES "fa80::0202:b3ff:fe1e:9000 fa80::0202:b3ff:fe1e:9001 fa80::0202:b3ff:fe1e:9002")
 *   -> IP6Encap(SRC fa80::0202:b3ff:fe1e:8329, DST f880::0202:b3ff:fe1e:0002, PROTO 43)
 *   -> EtherEncap(0x0800, 00:0a:95:9d:68:16, 00:0a:95:9d:68:17)
 *   -> ToDump("ip6.dump")
 *
 */

class RoutingZeroEncap : public Element {

public:
    RoutingZeroEncap();
    ~RoutingZeroEncap();

    const char *class_name() const		{ return "RoutingZeroEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);
    
private:
    uint8_t _next_header;               /* next header */
    uint8_t _header_ext_length;         /* 8-bit unsigned integer.  Length of the Routing
                                           header in 8-octet units, not including the first
                                           8 octets.  For the Type 0 Routing header, Hdr
                                           Ext Len is equal to two times the number of
                                           addresses in the header. */
    uint8_t _segments_left;             /* 8-bit unsigned integer.  Number of route
                                           segments remaining, i.e., number of explicitly
                                           listed intermediate nodes still to be visited
                                           before reaching the final destination. */
    Vector<in6_addr> _ip6_addresses;  /* list containing the ipv6 addresses that must be visited */



};

CLICK_ENDDECLS
#endif /* ROUTING0ENCAP */

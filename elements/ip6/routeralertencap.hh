#ifndef CLICK_ROUTERALERTENCAP_HH
#define CLICK_ROUTERALERTENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * RouterAlertEncap([OFFSET])
 * =s ip6
 *
 * =d
 *
 * Encapsulates a packet in a Hop-by-Hop option header with
 * as option type IPv6 Router Alert Option.
 *
 * =e
 *
 * InfiniteSource(LIMIT 1)
 *   -> UDPEncap(1200,1500)
 *   -> RouterAlertEncap(PROTO 17, OPTION 0)
 *   -> IP6Encap(SRC fa80::0202:b3ff:fe1e:8329, DST f880::0202:b3ff:fe1e:0002, PROTO 0)
 *   -> EtherEncap(0x0800, 00:0a:95:9d:68:16, 00:0a:95:9d:68:17)
 *   -> ToDump("ip6.dump")
 *
 */

class RouterAlertEncap : public Element {

public:
    RouterAlertEncap();
    ~RouterAlertEncap();

    const char *class_name() const		{ return "RouterAlertEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);
    
private:
    uint8_t _next_header; // next header following the IPv6 Hop-by-Hop extension header.
    uint16_t _router_alert_option;  // if our Hop-By-Hop Extension header happens to be a router alert then this value represents the value of the router alert option (RFC 2711) 
                                    // 0 means Multicast Listener Discovery message, 1 means RSVP message, 2 means Active Network message, 3-65535 are reserved to IANA for further use.

};

CLICK_ENDDECLS
#endif

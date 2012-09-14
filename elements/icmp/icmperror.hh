#ifndef CLICK_ICMPERROR_HH
#define CLICK_ICMPERROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * ICMPError(SRC, TYPE [, CODE, I<keywords> BADADDRS, MTU])
 * =s icmp
 * generates ICMP error packets
 * =d
 * Generate an ICMP error packet, with specified TYPE and CODE,
 * in response to an incoming IP packet. The output is an IP/ICMP packet.
 * The ICMP packet's IP source address is set to SRC.
 * The error packet will include (as payload)
 * the original packet's IP header and an initial segment of its
 * IP payload. ICMPError sets the packet destination IP annotation and,
 * optionally, the fix_ip_src annotation.
 *
 * TYPE and CODE may be integers between 0 and 255 or mnemonic names; CODE
 * defaults to 0. Valid named TYPEs are `unreachable' [3], `sourcequench' [4],
 * `redirect' [5], `timeexceeded' [11], and `parameterproblem' [12]. Valid
 * named CODEs are `net' [0], `host' [1], `protocol' [2], `port' [3], and
 * `needfrag' [4] for `unreachable'; `net' [0] and `host' [1] for `redirect';
 * `transit' [0] and `reassembly' [1] for `timeexceeded'; and `erroratptr'
 * [0], `missingopt' [1], and `length' [2] for `parameterproblem'.
 *
 * The intent is that elements that give rise to errors, like DecIPTTL,
 * should have two outputs, one of which is connected to an ICMPError.
 * Perhaps the ICMPError should be followed by a rate limiting
 * element.
 *
 * ICMPError never generates a packet in response to an ICMP error packet, a
 * fragment, or a link broadcast. The BADADDRS keyword argument supplies an
 * optional list of bad IP addresses; if it is present, then ICMPError doesn't
 * generate packets in response to packets with one of those addresses as
 * either source or destination.
 *
 * The output of ICMPError should be connected to the routing lookup
 * machinery, much as if the ICMP errors came from a hardware interface.
 *
 * If TYPE is 12 (`parameterproblem') and CODE is 0 (`erroratptr'), ICMPError
 * takes the error pointer from the packet's ICMP parameter problem
 * annotation. The IPGWOptions element sets the annotation.
 *
 * If TYPE is 5 (`redirect'), produces an ICMP redirect message. The gateway
 * address is taken from the destination annotation. Usually a Paint-PaintTee
 * element pair hands the packet to a redirect ICMPError. RFC1812 says only
 * code 1 (`host') should be used.
 *
 * If TYPE is 3 ('unreachable') and CODE is 4 ('needfrag'), produces
 * an ICMP unreachable message containing the MTU of the next-hop
 * interface, specified by the PMTU keyword. This error should be
 * returned to the sender of an IP packet with the DF bit set, that is
 * longer than the next-hop MTU, to support Path MTU Discovery.
 *
 * If the input packet has a source route option, the output packet will also
 * have a source route option, containing the routers from the input source
 * route, reversed.
 *
 * Will not generate a packet larger than MTU, which defaults to 576.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item BADADDRS
 *
 * A list of source IP addresses for which ICMP errors should not be
 * generated.  Usually the limited broadcast address(es) of the current
 * subnet.
 *
 * =item MTU
 *
 * Unsigned.  The maximum error packet size generated.  Defaults to 576.
 *
 * =item PMTU
 *
 * Unsigned.  The maximum MTU for the path, reported in unreachable messages.
 *
 * =item USE_FIX_ANNO
 *
 * Boolean.  If false, do not set the fix_ip_src annotation on output packets.
 * Defaults to true.
 *
 * =back
 *
 * =e
 * This configuration fragment produces ICMP Time Exceeded error
 * messages in response to TTL expirations, but limits the
 * rate at which such messages can be sent to 10 per second:
 *
 *   dt : DecIPTTL;
 *   dt[1] -> ICMPError(18.26.4.24, timeexceeded) -> m :: RatedSplitter(10) -> ...
 *   m[1] -> Discard;
 *
 * =n
 *
 * ICMPError can't decide if the packet's source or destination address is an
 * IP directed broadcast address; it is supposed to ignore packets with such
 * addresses.
 *
 * =a DecIPTTL, FixIPSrc, IPGWOptions */

class ICMPError : public Element { public:

    ICMPError() CLICK_COLD;
    ~ICMPError() CLICK_COLD;

    const char *class_name() const		{ return "ICMPError"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    IPAddress _src_ip;
    int _type;
    int _code;
    Vector<IPAddress> _bad_addrs;
    unsigned _mtu;
    unsigned _pmtu;
    bool _use_fix_anno;

    static bool is_error_type(int);
    bool unicast(struct in_addr) const;
    bool valid_source(struct in_addr) const;
    static const uint8_t *valid_source_route(const click_ip *ip);

};

CLICK_ENDDECLS
#endif

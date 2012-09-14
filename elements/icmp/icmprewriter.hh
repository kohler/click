#ifndef CLICK_ICMPREWRITER_HH
#define CLICK_ICMPREWRITER_HH
#include <click/element.hh>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include "elements/ip/iprewriterbase.hh"
CLICK_DECLS

/*
=c

ICMPRewriter(MAPS, I<keywords> DST_ANNO)

=s nat

rewrites ICMP packets based on IP rewriter mappings

=d

Rewrites ICMP error packets by changing their source and/or destination
addresses and some of their payloads. It checks MAPS, a space-separated list
of IPRewriter-like elements, to see how to rewrite. This lets source quenches,
redirects, TTL-expired messages, and so forth pass through a NAT gateway.

ICMP error packets are sent in response to normal IP packets, and include a
small portion of the relevant IP packet data. If the IP packet had been sent
through IPRewriter, ICMPPingRewriter, or a similar element, then the ICMP
packet will be in response to the rewritten address. ICMPRewriter takes such
ICMP error packets and checks a list of IPRewriters for a relevant mapping. If
a mapping is found, ICMPRewriter will rewrite the ICMP packet so it appears
like a response to the original packet and emit the result on output 0.

ICMPRewriter may have one or two outputs. If it has one, then any
non-rewritten ICMP error packets, and any ICMP packets that are not errors,
are dropped. If it has two, then these kinds of packets are emitted on output
1.

Keyword arguments are:

=over 8

=item DST_ANNO

Boolean. If true, then set the destination IP address annotation on passing
packets to the rewritten destination address. Default is true.

=back

=n

ICMPRewriter supports the following ICMP types: destination unreachable, time
exceeded, parameter problem, source quench, and redirect.

MAPS elements may have element class IPAddrRewriter, IPRewriter, TCPRewriter,
ICMPPingRewriter, or other related classes.

=a

IPAddrRewriter, IPRewriter, ICMPPingRewriter, TCPRewriter */

class ICMPRewriter : public Element { public:

    ICMPRewriter() CLICK_COLD;
    ~ICMPRewriter() CLICK_COLD;

    const char *class_name() const	{ return "ICMPRewriter"; }
    const char *port_count() const	{ return "1/1-"; }
    const char *processing() const	{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    void push(int, Packet *);

  protected:

    struct MapEntry {
	MapEntry(IPRewriterBase *e, int po) : _elt(e), _port_offset(po) {}
	IPRewriterBase *_elt;
	int _port_offset;
    };

    enum {
	unmapped_output = -1
    };

    Vector<MapEntry> _maps;
    unsigned _annos;

    void rewrite_packet(WritablePacket *, click_ip *, click_udp *,
			const IPFlowID &, IPRewriterEntry *);
    void rewrite_ping_packet(WritablePacket *, click_ip *, click_icmp_echo *,
			     const IPFlowID &, IPRewriterEntry *);

    int handle(WritablePacket *p);

};

CLICK_ENDDECLS
#endif

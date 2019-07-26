#ifndef CLICK_IPREWRITER_HH
#define CLICK_IPREWRITER_HH
#include "tcprewriter.hh"
#include "udprewriter.hh"
CLICK_DECLS
class UDPRewriter;

/*
=c

IPRewriter(INPUTSPEC1, ..., INPUTSPECn [, I<keywords>])

=s nat

rewrites TCP/UDP packets' addresses and ports

=d

Rewrites the source address, source port, destination address, and/or
destination port on TCP and UDP packets, along with their checksums.
IPRewriter implements the functionality of a network address/port translator
E<lparen>NAPT).  See also IPAddrRewriter and IPAddrPairRewriter, which
implement Basic NAT, and TCPRewriter, which implements NAPT plus sequence
number changes for TCP packets.

IPRewriter maintains a I<mapping table> that records how packets are
rewritten.  The mapping table is indexed by I<flow identifier>, the quintuple
of source address, source port, destination address, destination port, and IP
protocol (TCP or UDP).  Each mapping contains a new flow identifier and an
output port.  Input packets with the indexed flow identifier are rewritten to
use the new flow identifier, then emitted on the output port.  A mapping is
written as follows:

    (SA, SP, DA, DP, PROTO) => (SA', SP', DA', DP') [*OUTPUT]

When IPRewriter receives a packet, it first looks up that packet in the
mapping table by flow identifier.  If the table contains a mapping for the
input packet, then the packet is rewritten according to the mapping and
emitted on the specified output port.  If there was no mapping, the packet is
handled by the INPUTSPEC corresponding to the input port on which the packet
arrived.  (There are as many input ports as INPUTSPECs.)  Most INPUTSPECs
install new mappings, so that future packets from the same TCP or UDP flow are
handled by the mapping table rather than some INPUTSPEC.  The six forms of
INPUTSPEC handle input packets as follows:

=over 5

=item 'drop' or 'discard'

Discards input packets.

=item 'pass OUTPUT'

Sends input packets to output port OUTPUT.  No mappings are installed.

=item 'keep FOUTPUT ROUTPUT'

Installs mappings that preserve the input packet's flow ID.  Specifically,
given an input packet with flow ID (SA, SP, DA, DP, PROTO), two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA, SP, DA, DP) [*FOUTPUT]
    (DA, DP, SA, SP, PROTO) => (DA, DP, SA, SP) [*ROUTPUT]

Thus, the input packet is emitted on output port FOUTPUT unchanged, and
packets from the reply flow are emitted on output port ROUTPUT unchanged.

=item 'pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'

Creates a mapping according to the given pattern, 'SADDR SPORT DADDR DPORT'.
Any pattern field may be a dash '-', in which case the packet's corresponding
field is left unchanged.  For instance, the pattern '1.0.0.1 20 - -' will
rewrite input packets' source address and port, but leave its destination
address and port unchanged.  SPORT may be a port range 'L-H'; IPRewriter will
choose a source port in that range so that the resulting mappings don't
conflict with any existing mappings.  The input packet's source port is
preferred, if it is available; otherwise a random port is chosen.  If no
source port is available, the packet is dropped.  To allocate source ports
sequentially (which can make testing easier), append a pound sign to the
range, as in '1024-65535#'.  To choose a random port rather than preferring
the source, append a '?'.

Say a packet with flow ID (SA, SP, DA, DP, PROTO) is received, and the
corresponding new flow ID is (SA', SP', DA', DP').  Then two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA', SP', DA', DP') [*FOUTPUT]
    (DA', DP', SA', SP', PROTO) => (DA, DP, SA, SP) [*ROUTPUT]

Thus, the input packet is rewritten and sent to FOUTPUT, and packets from the
reply flow are rewritten to look like part of the original flow and sent to
ROUTPUT.

=item 'pattern PATNAME FOUTPUT ROUTPUT'

Like 'pattern' above, but refers to named patterns defined by an
IPRewriterPatterns element.

=item 'ELEMENTNAME'

Creates mappings according to instructions from the element ELEMENTNAME.  This
element must implement the IPMapper interface.  One example mapper is
RoundRobinIPMapper.

=back

IPRewriter has no mappings when first initialized.

Input packets must have their IP header annotations set.  Non-TCP and UDP
packets, and second and subsequent fragments, are dropped unless they arrive
on a 'pass' input port.  IPRewriter changes IP packet data and, optionally,
destination IP address annotations; see the DST_ANNO keyword argument below.

Keyword arguments determine how often stale mappings should be removed.

=over 5

=item TCP_TIMEOUT I<time>

Time out TCP connections every I<time> seconds. Defaults to 24 hours. This
timeout applies to TCP connections for which payload data has been seen
flowing in both directions.

=item TCP_DONE_TIMEOUT I<time>

Time out completed TCP connections every I<time> seconds. Defaults to 4
minutes. FIN and RST flags mark TCP connections as complete.

=item TCP_NODATA_TIMEOUT I<time>

Time out non-bidirectional TCP connections every I<time> seconds. Defaults to
5 minutes. A non-bidirectional TCP connection is one in which payload data
hasn't been seen in at least one direction. This should generally be larger
than TCP_DONE_TIMEOUT.

=item TCP_GUARANTEE I<time>

Preserve each TCP connection mapping for at least I<time> seconds after each
successfully processed packet. Defaults to 5 seconds. Incoming flows are
dropped if an IPRewriter's mapping table is full of guaranteed flows.

=item UDP_TIMEOUT I<time>

Time out UDP connections every I<time> seconds. Default is 5 minutes.

=item UDP_STREAMING_TIMEOUT I<time>

Timeout UDP streaming connections every I<time> seconds. A "streaming"
connection, in contrast to an "RPC-like" connection, comprises at least 3
packets and at least one packet in each direction. Default is the UDP_TIMEOUT
setting.

=item UDP_GUARANTEE I<time>

UDP connection mappings are guaranteed to exist for I<time> seconds after each successfully processed packet. Defaults to 5 seconds.

=item REAP_INTERVAL I<time>

Reap timed-out connections every I<time> seconds. Default is 15 minutes.

=item MAPPING_CAPACITY I<capacity>

Set the maximum number of mappings this rewriter can hold to I<capacity>.
I<Capacity> can either be an integer or the name of another rewriter-like
element, in which case this element will share the other element's capacity.

=item DST_ANNO

Boolean. If true, then set the destination IP address annotation on passing
packets to the rewritten destination address. Default is true.

=back

=h table_size r

Returns the number of mappings in this IPRewriter's tables.

=h mapping_failures r

Returns the number of mapping failures, which can occur, for example, when the
IPRewriter runs out of source ports, or when a new flow is dropped because the
IPRewriter is full.

=h size r

Returns the number of flows in the flow set.  This is generally the same as
'table_size', but can be more when several rewriters share a flow set.

=h capacity rw

Return or set the capacity of the flow set.  The returned value is two
space-separated numbers, where the first is the capacity and the second is the
short-term flow reservation.  When writing, the short-term reservation can be
omitted; it is then set to the minimum of 50 and one-eighth the capacity.

=h tcp_table read-only

Returns a human-readable description of the IPRewriter's current TCP mapping
table. An unparsed mapping includes both directions' output ports; the
relevant output port is starred.

=h udp_table read-only

Returns a human-readable description of the IPRewriter's current UDP mapping
table.

=h tcp_lookup read

Takes a TCP flow as a space-separated

    saddr sport daddr dport

and attempts to find a forward mapping for that flow. If found, rewrites the
flow and returns in the same format.  Otherwise, returns nothing.

=a TCPRewriter, IPAddrRewriter, IPAddrPairRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter */

class IPRewriter : public TCPRewriter { public:

    typedef UDPRewriter::UDPFlow UDPFlow;

    IPRewriter() CLICK_COLD;
    ~IPRewriter() CLICK_COLD;

    const char *class_name() const		{ return "IPRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    IPRewriterEntry *get_entry(int ip_p, const IPFlowID &flowid, int input);
    HashContainer<IPRewriterEntry> *get_map(int mapid) {
	if (mapid == IPRewriterInput::mapid_default)
	    return &_map;
	else if (mapid == IPRewriterInput::mapid_iprewriter_udp)
	    return &_udp_map;
	else
	    return 0;
    }
    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);
    click_jiffies_t best_effort_expiry(const IPRewriterFlow *flow) {
	if (flow->ip_p() == IP_PROTO_TCP)
	    return TCPRewriter::best_effort_expiry(flow);
	else
	    return flow->expiry() + udp_flow_timeout(static_cast<const UDPFlow *>(flow)) - _udp_timeouts[1];
    }

    void push(int, Packet *);

    void add_handlers() CLICK_COLD;

  private:

    Map _udp_map;
    SizedHashAllocator<sizeof(UDPFlow)> _udp_allocator;
    uint32_t _udp_timeouts[2];
    uint32_t _udp_streaming_timeout;

    int udp_flow_timeout(const UDPFlow *mf) const {
	if (mf->streaming())
	    return _udp_streaming_timeout;
	else
	    return _udp_timeouts[0];
    }

    static inline Map &reply_udp_map(IPRewriterInput *rwinput) {
	IPRewriter *x = static_cast<IPRewriter *>(rwinput->reply_element);
	return x->_udp_map;
    }
    static String udp_mappings_handler(Element *e, void *user_data);

};




CLICK_ENDDECLS
#endif

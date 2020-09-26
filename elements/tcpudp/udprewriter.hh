#ifndef CLICK_UDPREWRITER_HH
#define CLICK_UDPREWRITER_HH
#include "elements/ip/iprewriterbase.hh"
#include "elements/ip/iprwmapping.hh"
#include <click/sync.hh>
CLICK_DECLS

/*
=c

UDPRewriter(INPUTSPEC1, ..., INPUTSPECn [, I<keywords>])

=s nat

rewrites TCP/UDP packets' addresses and ports

=d

Rewrites the source address, source port, destination address, and/or
destination port on UDP packets, along with their checksums.  IPRewriter
implements the functionality of a network address/port translator
E<lparen>NAPT).  See also IPAddrRewriter and IPAddrPairRewriter, which
implement Basic NAT, and TCPRewriter, which implements NAPT plus sequence
number changes for TCP packets.

Despite its name, UDPRewriter will validly rewrite both TCP and UDP.  However,
in most uses, any given UDPRewriter will see packets of only one protocol.

UDPRewriter maintains a I<mapping table> that records how packets are
rewritten.  The mapping table is indexed by I<flow identifier>, the quadruple
of source address, source port, destination address, and destination port.
Each mapping contains a new flow identifier and an output port.  Input packets
with the indexed flow identifier are rewritten to use the new flow identifier,
then emitted on the output port.  A mapping is written as follows:

    (SA, SP, DA, DP) => (SA', SP', DA', DP') [OUTPUT]

When UDPRewriter receives a packet, it first looks up that packet in the
mapping table by flow identifier.  If the table contains a mapping for the
input packet, then the packet is rewritten according to the mapping and
emitted on the specified output port (but see the CONSTRAIN_FLOW keyword
argument).  If there was no mapping, the packet is handled by the INPUTSPEC
corresponding to the input port on which the packet arrived.  (There are as
many input ports as INPUTSPECs.)  Most INPUTSPECs install new mappings, so
that future packets from the same TCP or UDP flow are handled by the mapping
table rather than some INPUTSPEC.  The six forms of INPUTSPEC handle input
packets as follows:

=over 5

=item 'drop' or 'discard'

Discards input packets.

=item 'pass OUTPUT'

Sends input packets to output port OUTPUT.  No mappings are installed.

=item 'keep FOUTPUT ROUTPUT'

Installs mappings that preserve the input packet's flow ID.  Specifically,
given an input packet with flow ID (SA, SP, DA, DP, PROTO), two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA, SP, DA, DP) [FOUTPUT]
    (DA, DP, SA, SP, PROTO) => (DA, DP, SA, SP) [ROUTPUT]

Thus, the input packet is emitted on output port FOUTPUT unchanged, and
packets from the reply flow are emitted on output port ROUTPUT unchanged.

=item 'pattern SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'

Creates a mapping according to the given pattern, 'SADDR SPORT DADDR DPORT'.
Any pattern field may be a dash '-', in which case the packet's corresponding
field is left unchanged.  For instance, the pattern '1.0.0.1 20 - -' will
rewrite input packets' source address and port, but leave its destination
address and port unchanged.  SPORT may be a port range 'L-H'; UDPRewriter will
choose a source port in that range so that the resulting mappings don't
conflict with any existing mappings.  The input packet's source port is
preferred, if it is available; otherwise, a random port is chosen.  If no
source port is available, the packet is dropped.  To allocate source ports
sequentially (which can make testing easier), append a pound sign to the
range, as in '1024-65535#'.  To choose a random port rather than preferring
the source, append a '?'.

Say a packet with flow ID (SA, SP, DA, DP, PROTO) is received, and the
corresponding new flow ID is (SA', SP', DA', DP').  Then two mappings are
installed:

    (SA, SP, DA, DP, PROTO) => (SA', SP', DA', DP') [FOUTPUT]
    (DA', DP', SA', SP', PROTO) => (DA, DP, SA, SP) [ROUTPUT]

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

UDPRewriter has no mappings when first initialized.

Input packets must have their IP header annotations set.  Non-TCP and UDP
packets, and second and subsequent fragments, are dropped unless they arrive
on a 'pass' input port.  UDPRewriter changes IP packet data and, optionally,
destination IP address annotations; see the DST_ANNO keyword argument below.

Keyword arguments are:

=over 5

=item TIMEOUT I<time>

Time out connections every I<time> seconds. Default is 5 minutes.

=item STREAMING_TIMEOUT I<time>

Timeout streaming connections every I<time> seconds. A "streaming"
connection, in contrast to an "RPC-like" connection, comprises at least 3
packets and at least one packet in each direction. Default is the TIMEOUT
setting.

=item GUARANTEE I<time>

Preserve each connection mapping for at least I<time> seconds after each
successfully processed packet. Defaults to 5 seconds. Incoming flows are
dropped if a UDPRewriter's mapping table is full of guaranteed flows.

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

=h table read-only

Returns a human-readable description of the UDPRewriter's current mapping
table.

=a TCPRewriter, IPAddrRewriter, IPAddrPairRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter */

class UDPRewriter : public IPRewriterBase { public:

    class UDPFlow : public IPRewriterFlow { public:

	UDPFlow(IPRewriterInput *owner, const IPFlowID &flowid,
		const IPFlowID &rewritten_flowid, int ip_p,
		bool guaranteed, click_jiffies_t expiry_j)
	    : IPRewriterFlow(owner, flowid, rewritten_flowid,
			     ip_p, guaranteed, expiry_j) {
	}

	bool streaming() const {
	    return _tflags > 6;
	}

	void apply(WritablePacket *p, bool direction, unsigned annos);

    };

    UDPRewriter() CLICK_COLD;
    ~UDPRewriter() CLICK_COLD;

    const char *class_name() const		{ return "UDPRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);
    click_jiffies_t best_effort_expiry(const IPRewriterFlow *flow) {
	return flow->expiry() + udp_flow_timeout(static_cast<const UDPFlow *>(flow)) - _timeouts[1];
    }

    void push(int, Packet *);

    void add_handlers() CLICK_COLD;

  private:

    SizedHashAllocator<sizeof(UDPFlow)> _allocator;
    unsigned _annos;
    uint32_t _udp_streaming_timeout;

    int udp_flow_timeout(const UDPFlow *mf) const {
	if (mf->streaming())
	    return _udp_streaming_timeout;
	else
	    return _timeouts[0];
    }

    static String dump_mappings_handler(Element *, void *);

    friend class IPRewriter;

};




CLICK_ENDDECLS
#endif

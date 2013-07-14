// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPADDRREWRITER_HH
#define CLICK_IPADDRREWRITER_HH
#include "elements/ip/iprewriterbase.hh"
#include "elements/ip/iprwmapping.hh"
CLICK_DECLS

/*
=c

IPAddrRewriter(INPUTSPEC1, ..., INPUTSPECn)

=s nat

rewrites IP packets' addresses

=d

Rewrites the source and/or destination addresses on IP packets, along with
their checksums.  IPAddrRewriter implements the functionality of a network
address translator E<lparen>Basic NAT), where internal hosts are assigned
temporary IP addresses as they access the Internet.  Basic NAT works for any
IP protocol, but the number of internal hosts that can access the Internet
simultaneously is limited by the number of external IP addresses available.
See also IPRewriter and TCPRewriter, which implement network address/port
translation (NAPT).

IPAddrRewriter maintains a I<mapping table> that records how addresses are
rewritten.  On receiving a packet, IPAddrRewriter first looks up that packet
in the mapping table by source or destination address.  If the table contains
a mapping for either address, then the packet is rewritten according to the
mapping and emitted on the specified output port.  If there was no mapping,
the packet is handled by the INPUTSPEC corresponding to the input port on
which the packet arrived.  (There are as many input ports as INPUTSPECs.)
Most INPUTSPECs install new mappings, so that future packets from the same
address are handled by the mapping table rather than some INPUTSPEC.  The six
forms of INPUTSPEC handle input packets as follows:

=over 5

=item 'drop', 'pass OUTPUT', 'keep FOUTPUT ROUTPUT', 'ELEMENTNAME'

These INPUTSPECs behave like those in IPRewriter.

=item 'pattern SADDR[-SADDR2] - FOUTPUT ROUTPUT'

Creates a mapping according to the given pattern.  The destination
address must be a dash '-', since IPAddrRewriter only changes outgoing
packets' source addresses.  (If you want to rewrite destination addresses,
use IPAddrPairRewriter instead.)
SADDR may be a range 'L-H' or prefix 'ADDR/PFX';
IPRewriter will choose an unallocated address in that range, or drop the
packet if no address is available.  Normally addresses are chosen randomly
within the range.  To allocate addresses sequentially (which can make testing
easier), append a pound sign to the range, as in '1.0.0.1-1.255.255.254#'.
SADDR may also be a dash, in which case the source address is left unchanged.

Packets sent from the old source address are rewritten and sent to FOUTPUT,
and packets sent to the new source address are rewritten back and sent to
ROUTPUT.

=item 'pattern PATNAME FOUTPUT ROUTPUT'

Behaves like the version in IPRewriter, except that PATNAME must name an
IPAddrRewriter-like pattern.

=back

Input packets must have their IP header annotations set.  IPAddrRewriter
changes IP packet data and destination IP address annotations.

Keyword arguments are:

=over 5

=item TIMEOUT I<time>

Time out connections every I<time> seconds. Default is 5 minutes.

=item GUARANTEE I<time>

Preserve each connection mapping for at least I<time> seconds after each
successfully processed packet. Defaults to 5 seconds. Incoming flows are
dropped if the mapping table is full of guaranteed flows.

=item REAP_INTERVAL I<time>

Reap timed-out connections every I<time> seconds. Default is 15 minutes.

=item MAPPING_CAPACITY I<capacity>

Set the maximum number of mappings this rewriter can hold to I<capacity>.
I<Capacity> can either be an integer or the name of another rewriter-like
element, in which case this element will share the other element's capacity.

=back

=h table read-only

Returns a human-readable description of the IPAddrRewriter's current mapping
table.

=h table_size read-only

Returns the number of mappings in the table.

=h patterns read-only

Returns a human-readable description of the patterns associated with this
IPAddrRewriter.

=a IPRewriter, IPAddrPairRewriter, TCPRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter,
StoreIPAddress (for simple uses) */

class IPAddrRewriter : public IPRewriterBase { public:

    class IPAddrFlow : public IPRewriterFlow { public:

	IPAddrFlow(IPRewriterInput *owner, const IPFlowID &flowid,
		   const IPFlowID &rewritten_flowid,
		   bool guaranteed, click_jiffies_t expiry_j)
	    : IPRewriterFlow(owner, flowid, rewritten_flowid,
			     0, guaranteed, expiry_j) {
	}

	void apply(WritablePacket *p, bool direction, unsigned annos);

	void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;

    };

    IPAddrRewriter() CLICK_COLD;
    ~IPAddrRewriter() CLICK_COLD;

    const char *class_name() const		{ return "IPAddrRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    //void take_state(Element *, ErrorHandler *);

    inline IPRewriterEntry *get_entry(int ip_p, const IPFlowID &flowid, int input);
    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);

    void push(int, Packet *);

    void add_handlers() CLICK_COLD;

  protected:

    SizedHashAllocator<sizeof(IPAddrFlow)> _allocator;
    unsigned _annos;

    static String dump_mappings_handler(Element *, void *);

};


inline void
IPAddrRewriter::destroy_flow(IPRewriterFlow *flow)
{
    unmap_flow(flow, _map);
    static_cast<IPAddrFlow *>(flow)->~IPAddrFlow();
    _allocator.deallocate(flow);
}

CLICK_ENDDECLS
#endif

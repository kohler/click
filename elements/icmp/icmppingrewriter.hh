#ifndef CLICK_ICMPPINGREWRITER_HH
#define CLICK_ICMPPINGREWRITER_HH
#include "elements/ip/iprewriterbase.hh"
#include "elements/ip/iprwmapping.hh"
#include <clicknet/icmp.h>
CLICK_DECLS

/*
=c

ICMPPingRewriter(INPUTSPEC1, ..., INPUTSPECn, I<keywords> DST_ANNO, TIMEOUT)

=s nat

rewrites ICMP echo requests and replies

=d

Rewrites ICMP echo requests and replies by changing their source and/or
destination addresses and ICMP identifiers.  This lets pings pass through a
NAT gateway.

Expects ICMP echo requests and echo replies.  Each ICMP echo request is
rewritten according to the INPUTSPEC on its input port.  The INPUTSPEC may
change the packets' addresses.  Usually the INPUTSPEC will also specify a
source port range, which is used to change echo requests' identifiers.
Replies to the rewritten request are also rewritten to look like they were
responding to the original request.  ICMPPingRewriter optionally changes
destination IP address annotations; see the DST_ANNO keyword argument below.

ICMPPingRewriter actually keeps a table of mappings. Each mapping changes
a given (source address, destination address, identifier) triple into another
triple. Say that ICMPPingRewriter receives a request packet with triple
(I<src>, I<dst>, I<ident>), and chooses for it a new triple, (I<src2>,
I<dst2>, I<ident2>). The rewriter will then store two mappings in the table.
The first mapping changes requests (I<src>, I<dst>, I<ident>) into
requests (I<src2>, I<dst2>, I<ident2>). The second mapping changes
I<replies> (I<dst2>, I<src2>, I<ident2>) into replies (I<dst>, I<src>,
I<ident>).  Mappings are removed if they go unused for a default of 5 minutes.

Unexpected echo replies are dropped unless they arrive on an input with 'pass'
INPUTSPEC.

Keyword arguments are:

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

=item DST_ANNO

Boolean. If true, then set the destination IP address annotation on passing
packets to the rewritten destination address. Default is true.

=back

=a

IPRewriter, ICMPPingResponder */

class ICMPPingRewriter : public IPRewriterBase { public:

    class ICMPPingFlow : public IPRewriterFlow { public:

	ICMPPingFlow(IPRewriterInput *owner, const IPFlowID &flowid,
		     const IPFlowID &rewritten_flowid,
		     bool guaranteed, click_jiffies_t expiry_j)
	    : IPRewriterFlow(owner, flowid, rewritten_flowid,
			     IP_PROTO_ICMP, guaranteed, expiry_j) {
	    _udp_csum_delta = 0;
	    click_update_in_cksum(&_udp_csum_delta, flowid.sport(), rewritten_flowid.sport());
	}

	void apply(WritablePacket *p, bool direction, unsigned annos);

	void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;

    };

    ICMPPingRewriter() CLICK_COLD;
    ~ICMPPingRewriter() CLICK_COLD;

    const char *class_name() const	{ return "ICMPPingRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    IPRewriterEntry *get_entry(int ip_p, const IPFlowID &flowid, int input);
    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);

    void push(int, Packet *);

    void add_handlers() CLICK_COLD;

  private:

    SizedHashAllocator<sizeof(ICMPPingFlow)> _allocator;
    unsigned _annos;

    static String dump_mappings_handler(Element *, void *);

};


inline void
ICMPPingRewriter::destroy_flow(IPRewriterFlow *flow)
{
    unmap_flow(flow, _map);
    static_cast<ICMPPingFlow *>(flow)->~ICMPPingFlow();
    _allocator.deallocate(flow);
}

CLICK_ENDDECLS
#endif

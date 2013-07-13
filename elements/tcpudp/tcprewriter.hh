#ifndef CLICK_TCPREWRITER_HH
#define CLICK_TCPREWRITER_HH
#include "elements/ip/iprewriterbase.hh"
#include "elements/ip/iprwmapping.hh"
#include <clicknet/tcp.h>
CLICK_DECLS

/*
=c

TCPRewriter(INPUTSPEC1, ..., INPUTSPECn [, KEYWORDS])

=s nat

rewrites TCP packets' addresses, ports, and sequence numbers

=d

Rewrites TCP flows by changing their source address, source port, destination
address, and/or destination port, and optionally, their sequence numbers and
acknowledgement numbers. It also changes the destination IP address
annotation; see the DST_ANNO keyword argument below.

This element is an IPRewriter-like element. Please read the IPRewriter
documentation for more information and a detailed description of its
INPUTSPEC arguments.

In addition to IPRewriter's functionality, the TCPRewriter element can add or
subtract amounts from incoming packets' sequence and acknowledgement numbers,
including any SACK acknowledgement numbers. Each newly created mapping starts
with these deltas at zero; other elements can request changes to a given
mapping. For example, FTPPortMapper uses this facility.

Keyword arguments determine how often stale mappings should be removed.

=over 5

=item TCP_TIMEOUT I<time>

Time out TCP connections every I<time> seconds. Default is 24 hours. This
timeout applies to TCP connections for which payload data has been seen
flowing in both directions.

=item TCP_DONE_TIMEOUT I<time>

Time out completed TCP connections every I<time> seconds. Default is 4
minutes. FIN and RST flags mark TCP connections as complete.

=item TCP_NODATA_TIMEOUT I<time>

Time out non-bidirectional TCP connections every I<time> seconds. Default is 5
minutes. A non-bidirectional TCP connection is one in which payload data
hasn't been seen in at least one direction.

=item TCP_GUARANTEE I<time>

Preserve each TCP connection mapping for at least I<time> seconds after each
successfully processed packet. Defaults to 5 seconds. Incoming flows are
dropped if an TCPRewriter's mapping table is full of guaranteed flows.

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

Returns a human-readable description of the TCPRewriter's current mapping
table.

=h lookup

Takes a flow as a space-separated

    saddr sport daddr dport

and attempts to find a forward mapping for that flow. If found, rewrites the
flow and returns in the same format.  Otherwise, returns nothing.

=a IPRewriter, IPAddrRewriter, IPAddrPairRewriter, IPRewriterPatterns,
FTPPortMapper */

class TCPRewriter : public IPRewriterBase { public:

    class TCPFlow : public IPRewriterFlow { public:

	TCPFlow(IPRewriterInput *owner, const IPFlowID &flowid,
		const IPFlowID &rewritten_flowid,
		bool guaranteed, click_jiffies_t expiry_j)
	    : IPRewriterFlow(owner, flowid, rewritten_flowid,
			     IP_PROTO_TCP, guaranteed, expiry_j), _dt(0) {
	}

	~TCPFlow() {
	    while (delta_transition *x = _dt) {
		_dt = x->next();
		delete x;
	    }
	}

	enum {
	    s_forward_done = 1, s_reply_done = 2,
	    s_both_done = (s_forward_done | s_reply_done),
	    s_forward_data = 4, s_reply_data = 8,
	    s_both_data = (s_forward_data | s_reply_data)
	};
	bool both_done() const {
	    return (_tflags & s_both_done) == s_both_done;
	}
	bool both_data() const {
	    return (_tflags & s_both_data) == s_both_data;
	}

	int update_seqno_delta(bool direction, tcp_seq_t old_seqno, int32_t delta);
	tcp_seq_t new_seq(bool direction, tcp_seq_t seqno) const;
	tcp_seq_t new_ack(bool direction, tcp_seq_t seqno) const;

	void apply(WritablePacket *p, bool direction, unsigned annos);

	void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;

      private:

	struct delta_transition {
	    int32_t delta[2];
	    tcp_seq_t trigger[2];
	    uintptr_t nextptr;
	    delta_transition() {
		memset(this, 0, sizeof(delta_transition));
	    }
	    delta_transition *next() const {
		return reinterpret_cast<delta_transition *>(nextptr - (nextptr & 3));
	    }
	    bool has_trigger(bool direction) const {
		return nextptr & (1 << direction);
	    }
	};

	delta_transition *_dt;

	void apply_sack(bool direction, click_tcp *tcp, int transport_len);

    };

    TCPRewriter() CLICK_COLD;
    ~TCPRewriter() CLICK_COLD;

    const char *class_name() const		{ return "TCPRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);
    click_jiffies_t best_effort_expiry(const IPRewriterFlow *flow) {
	return flow->expiry() + tcp_flow_timeout(static_cast<const TCPFlow *>(flow)) - _timeouts[1];
    }

    void push(int, Packet *);

    void add_handlers() CLICK_COLD;

 protected:

    SizedHashAllocator<sizeof(TCPFlow)> _allocator;
    unsigned _annos;
    uint32_t _tcp_data_timeout;
    uint32_t _tcp_done_timeout;

    int tcp_flow_timeout(const TCPFlow *mf) const {
	if (mf->both_done())
	    return _tcp_done_timeout;
	else if (mf->both_data())
	    return _tcp_data_timeout;
	else
	    return _timeouts[0];
    }

    static String tcp_mappings_handler(Element *, void *);
    static int tcp_lookup_handler(int, String &str, Element *e, const Handler *h, ErrorHandler *errh);

};

inline void
TCPRewriter::destroy_flow(IPRewriterFlow *flow)
{
    unmap_flow(flow, _map);
    static_cast<TCPFlow *>(flow)->~TCPFlow();
    _allocator.deallocate(flow);
}

inline tcp_seq_t
TCPRewriter::TCPFlow::new_seq(bool direction, tcp_seq_t seqno) const
{
    delta_transition *dt = _dt;
    while (dt && dt->has_trigger(direction)
	   && !SEQ_GEQ(seqno, dt->trigger[direction]))
	dt = dt->next();
    return dt ? seqno + dt->delta[direction] : seqno;
}

inline tcp_seq_t
TCPRewriter::TCPFlow::new_ack(bool direction, tcp_seq_t ackno) const
{
    delta_transition *dt = _dt;
    while (dt && dt->has_trigger(!direction)
	   && !SEQ_GEQ(ackno - dt->delta[!direction], dt->trigger[!direction]))
	dt = dt->next();
    return dt ? ackno - dt->delta[!direction] : ackno;
}

CLICK_ENDDECLS
#endif

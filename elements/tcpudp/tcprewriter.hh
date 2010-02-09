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

=h mappings read-only

Returns a human-readable description of the TCPRewriter's current set of
mappings.

=a IPRewriter, IPAddrRewriter, IPAddrPairRewriter, IPRewriterPatterns,
FTPPortMapper */

class TCPRewriter : public IPRewriterBase { public:

    class TCPFlow : public IPRewriterFlow { public:

	TCPFlow(const IPFlowID &flowid, int output,
		const IPFlowID &rewritten_flowid, int reply_output,
		bool guaranteed, click_jiffies_t expiry_j,
		IPRewriterBase *owner, int owner_input)
	    : IPRewriterFlow(flowid, output, rewritten_flowid, reply_output,
			     IP_PROTO_TCP, guaranteed, expiry_j,
			     owner, owner_input) {
	    _trigger[0] = _trigger[1] = 0;
	    _delta[0] = _delta[1] = _old_delta[0] = _old_delta[1] = 0;
	    _tflags = 0;
	}

	enum {
	    tf_seqno_delta = 1, tf_reply_seqno_delta = 2
	};

	int update_seqno_delta(bool direction, tcp_seq_t old_seqno, int32_t delta);
	tcp_seq_t new_seq(bool direction, tcp_seq_t seqno) const;
	tcp_seq_t new_ack(bool direction, tcp_seq_t seqno) const;

	void apply(WritablePacket *p, bool direction, unsigned annos);

	void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;

      private:

	tcp_seq_t _trigger[2];
	int32_t _delta[2];
	int32_t _old_delta[2];

	void apply_sack(bool direction, click_tcp *tcp, int transport_len);

    };

    TCPRewriter();
    ~TCPRewriter();

    const char *class_name() const		{ return "TCPRewriter"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *);

    IPRewriterEntry *add_flow(int ip_p, const IPFlowID &flowid,
			      const IPFlowID &rewritten_flowid, int input);
    void destroy_flow(IPRewriterFlow *flow);
    click_jiffies_t best_effort_expiry(IPRewriterFlow *flow) {
	return flow->expiry() + tcp_flow_timeout(static_cast<TCPFlow *>(flow)) - _timeouts[1];
    }

    void push(int, Packet *);

    void add_handlers();

 protected:

    SizedHashAllocator<sizeof(TCPFlow)> _allocator;
    unsigned _annos;
    int _tcp_data_timeout;
    int _tcp_done_timeout;

    int tcp_flow_timeout(const TCPFlow *mf) const {
	if (mf->both_done())
	    return _tcp_done_timeout;
	else if (mf->both_data())
	    return _tcp_data_timeout;
	else
	    return _timeouts[0];
    }

    static String tcp_mappings_handler(Element *, void *);

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
    return seqno + (SEQ_GEQ(seqno, _trigger[direction]) ? _delta[direction] : _old_delta[direction]);
}

inline tcp_seq_t
TCPRewriter::TCPFlow::new_ack(bool direction, tcp_seq_t ackno) const
{
    tcp_seq_t mod_ackno = ackno - _delta[!direction];
    return (SEQ_GEQ(mod_ackno, _trigger[!direction]) ? mod_ackno : ackno - _old_delta[!direction]);
}

CLICK_ENDDECLS
#endif

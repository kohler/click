// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPADDRPAIRREWRITER_HH
#define CLICK_IPADDRPAIRREWRITER_HH
#include "elements/ip/iprw.hh"
CLICK_DECLS

/*
=c

IPAddrPairRewriter(INPUTSPEC1, ..., INPUTSPECn)

=s nat

rewrites IP packets' addresses by address pair

=d

Rewrites the source and/or destination addresses on IP packets, along with
their checksums.  IPAddrPairRewriter implements per-address-pair network
address translation, a midpoint between Basic NAT (see IPAddrRewriter) and
NAPT (see IPRewriter and TCPRewriter).

IPAddrPairRewriter maintains a I<mapping table> that records how addresses are
rewritten.  On receiving a packet, IPAddrPairRewriter first looks up that
packet in the mapping table by source/destination address pair.  If the table
contains a mapping, then the packet is rewritten according to the mapping and
emitted on the specified output port.  If there was no mapping, the packet is
handled by the INPUTSPEC corresponding to the input port on which the packet
arrived.  (There are as many input ports as INPUTSPECs.)  Most INPUTSPECs
install new mappings, so that future packets from the same address are handled
by the mapping table rather than some INPUTSPEC.  The six forms of INPUTSPEC
handle input packets as follows:

=over 5

=item 'drop', 'pass OUTPUT', 'keep FOUTPUT ROUTPUT', 'ELEMENTNAME'

These INPUTSPECs behave like those in IPRewriter.

=item 'pattern SADDR[-SADDR2] DADDR FOUTPUT ROUTPUT'

Creates a mapping according to the given pattern, 'SADDR DADDR'.  Either
pattern field may be a dash '-', in which case the corresponding field is left
unchanged.  For instance, the pattern '1.0.0.1 -' will rewrite input packets'
source address, but leave its destination address unchanged.  SADDR may be a
range 'L-H' or prefix 'ADDR/PFX'; IPRewriter will choose an unallocated
address in that range, or drop the packet if no address is available.
Normally addresses are chosen randomly within the range.  To allocate
addresses sequentially (which can make testing easier), append a pound sign to
the range, as in '1.0.0.1-1.255.255.254#'.

Say a packet with address pair (SA, DA) is received, and the corresponding new
addresses are (SA', DA').  Then two mappings are installed:

    (SA, DA) => (SA', DA') [FOUTPUT]
    (DA', SA') => (DA, SA) [ROUTPUT]

Thus, the input packet is rewritten and sent to FOUTPUT, and packets from the
reply flow are rewritten to look like part of the original flow and sent to
ROUTPUT.

=item 'pattern PATNAME FOUTPUT ROUTPUT'

Behaves like the version in IPRewriter, except that PATNAME must name an
IPAddrRewriter-like pattern.

=back

Input packets must have their IP header annotations set.  IPAddrRewriter
changes IP packet data and destination IP address annotations.

=h mappings read-only

Returns a human-readable description of the IPAddrRewriter's current set of
mappings.

=h nmappings read-only

Returns the number of currently installed mappings.

=h patterns read-only

Returns a human-readable description of the patterns associated with this
IPAddrRewriter.

=a IPRewriter, IPAddrRewriter, TCPRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter,
StoreIPAddress (for simple uses) */

class IPAddrPairRewriter : public IPRw { public:

    class IPAddrPairMapping : public Mapping { public:

	IPAddrPairMapping(bool dst_anno)	: Mapping(dst_anno) { }

	IPAddrPairMapping *reverse() const { return static_cast<IPAddrPairMapping *>(reverse()); }

	void apply(WritablePacket *p);

	String unparse() const;
    
    };

    IPAddrPairRewriter();
    ~IPAddrPairRewriter();

    const char *class_name() const		{ return "IPAddrPairRewriter"; }
    void *cast(const char *);
    const char *port_count() const		{ return "1-/1-256"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    //void take_state(Element *, ErrorHandler *);

    void run_timer(Timer *);

    int notify_pattern(Pattern *, ErrorHandler *);
    IPAddrPairMapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
    Mapping *get_mapping(int, const IPFlowID &) const;

    void push(int, Packet *);

    void add_handlers();

  private:

    Map _map;

    Vector<InputSpec> _input_specs;
    Timer _timer;

    enum { GC_INTERVAL_SEC = 7200 };

    static String dump_mappings_handler(Element *, void *);
    static String dump_nmappings_handler(Element *, void *);
    static String dump_patterns_handler(Element *, void *);

};


inline IPRw::Mapping *
IPAddrPairRewriter::get_mapping(int, const IPFlowID &in_flow) const
{
    return _map[IPFlowID(in_flow.saddr(), 0, in_flow.daddr(), 0)];
}

CLICK_ENDDECLS
#endif

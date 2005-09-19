// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPADDRREWRITER_HH
#define CLICK_IPADDRREWRITER_HH
#include "elements/ip/iprw.hh"
CLICK_DECLS

/*
=c

IPAddrRewriter(INPUTSPEC1, ..., INPUTSPECn)

=s TCP

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

Creates a mapping according to the given pattern, 'SADDR -'.  The destination
address must be a dash '-', since IPAddrRewriter only changes outgoing
packets' source addresses.  SADDR may be a range 'L-H' or prefix 'ADDR/PFX';
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

=h mappings read-only

Returns a human-readable description of the IPAddrRewriter's current set of
mappings.

=h nmappings read-only

Returns the number of currently installed mappings.

=h patterns read-only

Returns a human-readable description of the patterns associated with this
IPAddrRewriter.

=a IPRewriter, IPAddrPairRewriter, TCPRewriter, IPRewriterPatterns,
RoundRobinIPMapper, FTPPortMapper, ICMPRewriter, ICMPPingRewriter */

class IPAddrRewriter : public IPRw { public:

    class IPAddrMapping : public Mapping { public:

	IPAddrMapping(bool dst_anno)	: Mapping(dst_anno) { }

	IPAddrMapping *reverse() const { return static_cast<IPAddrMapping *>(reverse()); }

	void apply(WritablePacket *p);

	String unparse() const;
    
    };

    IPAddrRewriter();
    ~IPAddrRewriter();

    const char *class_name() const		{ return "IPAddrRewriter"; }
    void *cast(const char *);
    const char *port_count() const		{ return "1-/1-256"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    //void take_state(Element *, ErrorHandler *);

    void run_timer();

    int notify_pattern(Pattern *, ErrorHandler *);
    IPAddrMapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
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
IPAddrRewriter::get_mapping(int, const IPFlowID &in_flow) const
{
    IPFlowID flow(in_flow.saddr(), 0, IPAddress(0), 0);
    if (IPRw::Mapping *m = _map[flow])
	return m;
    IPFlowID rev(IPAddress(0), 0, in_flow.daddr(), 0);
    return _map[rev];
}

CLICK_ENDDECLS
#endif

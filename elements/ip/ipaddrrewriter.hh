// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPADDRREWRITER_HH
#define CLICK_IPADDRREWRITER_HH
#include "elements/ip/iprw.hh"

/*
=c

IPAddrRewriter(INPUTSPEC1, ..., INPUTSPECn)

=s TCP

rewrites IP packets' addresses

=d

Rewrites IP packets by changing their source and/or destination addresses.

Has one or more inputs and one or more outputs. Input packets must have their
IP header annotations set. Output packets are valid IP packets; for instance,
rewritten packets have their checksums incrementally updated. However,
IPAddrRewriter does not change the destination IP address annotation.

IPAddrRewriter implements Basic NAT, where internal hosts are assigned
temporary IP addresses as they access the Internet. Basic NAT works for any IP
protocol, but the number of internal hosts that can access the Internet
simultaneously is limited by the number of external IP addresses available.
For NAPT (network address port translation), the more commonly implemented
version of NAT nowadays, see IPRewriter and TCPRewriter.

When it is first initialized, IPAddrRewriter has no mappings. Mappings are
created on the fly as new flows are encountered in the form of packets with
unknown IP addresses. This process is controlled by the INPUTSPECs. There are
as many input ports as INPUTSPEC configuration arguments. Each INPUTSPEC
specifies whether and how a mapping should be created when a new flow is
encountered on the corresponding input port. There are six forms of
INPUTSPEC:

=over 5

=item `drop', `nochange OUTPUT', `keep FOUTPUT ROUTPUT', `ELEMENTNAME'

These INPUTSPECs behave like those in IPRewriter.

=item `pattern SADDR[-SADDR2] DADDR FOUTPUT ROUTPUT'

Packets with no existing mapping are rewritten according to the given pattern.
IPAddrRewriter patterns are like IPRewriter patterns minus the source and
destination ports. Additionally, the source address can be a range of IP
addresses, SADDR-SADDR2, in which case a new IP address is chosen for each
unique source address. The two addresses SADDR and SADDR2 must lie within a
single /16 network.

A new mapping is installed. Packets with source address like the input
packet's are rewritten and sent to FOUTPUT; packets sent to the input packet's
source address are rewritten and sent to ROUTPUT.

=item `pattern PATNAME FOUTPUT ROUTPUT'

Behaves like the version in IPRewriter, except that PATNAME must name an
IPAddrRewriter-like pattern.

=back

=h mappings read-only

Returns a human-readable description of the IPAddrRewriter's current set of
mappings.

=h nmappings read-only

Returns the number of currently installed mappings.

=h patterns read-only

Returns a human-readable description of the patterns associated with this
IPAddrRewriter.

=a TCPRewriter, IPRewriterPatterns, RoundRobinIPMapper, FTPPortMapper,
ICMPRewriter, ICMPPingRewriter */

class IPAddrRewriter : public IPRw { public:

    class IPAddrMapping : public Mapping { public:

	IPAddrMapping(bool dst_anno)	: Mapping(dst_anno) { }

	IPAddrMapping *reverse() const { return static_cast<IPAddrMapping *>(reverse()); }

	void apply(WritablePacket *p);

	String s() const;
    
    };

    IPAddrRewriter();
    ~IPAddrRewriter();

    const char *class_name() const		{ return "IPAddrRewriter"; }
    void *cast(const char *);
    const char *processing() const		{ return PUSH; }
    IPAddrRewriter *clone() const		{ return new IPAddrRewriter; }
    void notify_noutputs(int);

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    //void take_state(Element *, ErrorHandler *);

    void run_scheduled();

    int notify_pattern(Pattern *, ErrorHandler *);
    IPAddrMapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
    Mapping *get_mapping(int, const IPFlowID &) const;

    void push(int, Packet *);

    void add_handlers();

  private:

    Map _map;

    Vector<InputSpec> _input_specs;
    Timer _timer;

    static const int GC_INTERVAL_SEC = 7200;

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

#endif

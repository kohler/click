// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEIPFLOWS_HH
#define CLICK_AGGREGATEIPFLOWS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/hashtable.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS
class HandlerCall;

/*
=c

AggregateIPFlows([I<KEYWORDS>])

=s ipmeasure

sets aggregate annotation based on flow

=d

AggregateIPFlows monitors TCP and UDP flows, setting the aggregate annotation
on every passing packet to a flow number, and the paint annotation to a
direction indication. Non-TCP/UDP packets and short packets are emitted on
output 1, or dropped if there is no output 1.

AggregateIPFlows uses source and destination addresses and source and
destination ports to distinguish flows. Reply packets get the same flow
number, but a different paint annotation. Old flows die after a configurable
timeout, after which new packets with the same addresses and ports get a new
flow number. UDP, active TCP, and completed TCP flows have different timeouts.

Flow numbers are assigned sequentially, starting from 1. Different flows get
different numbers. Paint annotations are set to 0 or 1, depending on whether
packets are on the forward or reverse subflow. (The first packet seen on each
flow gets paint color 0; reply packets get paint color 1. ICMP errors get
paints 2 and 3.)

AggregateIPFlows can optionally apply aggregate annotations to ICMP errors.
See the ICMP keyword argument below.

If FRAGMENTS is true (the default in push context), AggregateIPFlows assigns
aggregate annotations to second and subsequent fragments. It does this by
holding onto the fragments until it finds the relevant port numbers, which are
attached to the first fragment. Fragments for which no port numbers can be
found after FRAGMENT_TIMEOUT seconds of packet time are emitted on port 1 or
dropped.

Fragment processing may cause AggregateIPFlows to reorder packets. Packets
sent between any given pair of addresses are always emitted in the order in
which they were received, but packets sent between different address pairs may
come out in a different relative order.

Fragment processing also causes AggregateIPFlows to store packets. If you
aren't careful, those stored packets might remain in the AggregateIPFlows
element when the driver quits, which is probably not what you want. The
Example below shows one way to manage potentially lingering fragments.

If FRAGMENTS is false, second and subsequent fragments are emitted on port 1
or dropped.

Keywords are:

=over 8

=item TRACEINFO

Filename. If provided, output information about each flow to that filename in
an XML format. Only available at userlevel.

=item SOURCE

Element. If provided, the results of that element's 'C<filename>' and
'C<packet_filepos>' read handlers will be recorded in the TRACEINFO dump. (It
is not an error if the element doesn't have those handlers.) The
'C<packet_filepos>' results may be particularly useful, since a reader can use
those results to skip ahead through a trace file. Only available at userlevel.

=item TCP_TIMEOUT

The timeout for active TCP flows, in seconds. Default is 24 hours.

=item TCP_DONE_TIMEOUT

The timeout for completed TCP flows, in seconds. A completed TCP flow has seen
FIN flags on both subflows. Default is 30 seconds.

=item UDP_TIMEOUT

The timeout for UDP connections, in seconds. Default is 1 minute.

=item FRAGMENT_TIMEOUT

The timeout for fragments, in seconds, Default is 30 seconds.

=item REAP

The garbage collection interval. Default is 20 minutes of packet time.

=item ICMP

Boolean. If true, then mark ICMP errors relating to a connection with an
aggregate annotation corresponding to that connection. ICMP error packets get
paint annotations equal to 2 plus the paint color of the encapsulated packet.
Default is false.

=item FRAGMENTS

Boolean. If true, then try to assign aggregate annotations to all fragments.
May only be set to true if AggregateIPFlows is running in a push context.
Default is true in a push context and false in a pull context.

=back

AggregateIPFlows is an AggregateNotifier, so AggregateListeners can request
notifications when new aggregates are created and old ones are deleted.

=h clear write-only

Clears all flow information. Future packets will get new aggregate annotation
values. This may cause packets to be emitted if FRAGMENTS is true.

=e

This configuration counts the number of packets in each flow in a trace, using
AggregateCounter.

   FromDump(tracefile.dump, STOP true, FORCE_IP true)
       -> af :: AggregateIPFlows
       -> AggregateCounter
       -> Discard;
   DriverManager(wait, write af.clear)

The DriverManager line handles fragments that might be left in the
AggregateIPFlows element after FromDump completes. Again, AggregateIPFlows
collects fragments as they arrive and emits them later. If the last packets in
FromDump are fragments, then AggregateIPFlows will still be hanging onto them
when FromDump stops the driver. The DriverManager element waits for FromDump
to request a driver stop, then calls the C<af.clear> handler to flush any
remaining fragments.

=a

AggregateIP, AggregateIPAddrPair, AggregateCounter, DriverManager */

class AggregateIPFlows : public Element, public AggregateNotifier { public:

    AggregateIPFlows() CLICK_COLD;
    ~AggregateIPFlows() CLICK_COLD;

    const char *class_name() const	{ return "AggregateIPFlows"; }
    void *cast(const char *);
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

#if CLICK_USERLEVEL
    bool stats() const			{ return _traceinfo_file; }
#endif

    void push(int, Packet *);
    Packet *pull(int);

    struct HostPair {
	uint32_t a;
	uint32_t b;
	HostPair() : a(0), b(0) { }
	HostPair(uint32_t aa, uint32_t bb) {
	    aa > bb ? (a = bb, b = aa) : (a = aa, b = bb);
	}
	inline hashcode_t hashcode() const;
    };

  private:

    struct FlowInfo {
	uint32_t _ports;
	uint32_t _aggregate;
	Timestamp _last_timestamp;
	unsigned _flow_over : 2;
	bool _reverse : 1;
	FlowInfo *_next;
	// have 24 bytes; statistics would double
	// + 8 + 8 = 16 bytes
	FlowInfo(uint32_t ports, FlowInfo *next, uint32_t agg) : _ports(ports), _aggregate(agg), _flow_over(0), _next(next) { }
	uint32_t aggregate() const { return _aggregate; }
	bool reverse() const	{ return _reverse; }
    };

#if CLICK_USERLEVEL
    struct StatFlowInfo : public FlowInfo {
	Timestamp _first_timestamp;
	uint32_t _filepos;
	uint32_t _packets[2];
	StatFlowInfo(uint32_t ports, FlowInfo *next, uint32_t agg) : FlowInfo(ports, next, agg) { _packets[0] = _packets[1] = 0; }
    };
#endif

    struct HostPairInfo {
	FlowInfo *_flows;
	Packet *_fragment_head;
	Packet *_fragment_tail;
	HostPairInfo() : _flows(0), _fragment_head(0), _fragment_tail(0) { }
	FlowInfo *find_force(uint32_t ports);
    };

    typedef HashTable<HostPair, HostPairInfo> Map;
    Map _tcp_map;
    Map _udp_map;

    uint32_t _next;
    unsigned _active_sec;
    unsigned _gc_sec;

    uint32_t _tcp_timeout;
    uint32_t _tcp_done_timeout;
    uint32_t _udp_timeout;
    uint32_t _smallest_timeout;

    unsigned _gc_interval;
    unsigned _fragment_timeout;

    bool _handle_icmp_errors : 1;
    unsigned _fragments : 2;
    bool _timestamp_warning : 1;

#if CLICK_USERLEVEL
    FILE *_traceinfo_file;
    String _traceinfo_filename;

    Element *_packet_source;
    HandlerCall *_filepos_h;
#endif

    static const click_ip *icmp_encapsulated_header(const Packet *);

    void clean_map(Map &);
    void reap_map(Map &, uint32_t, uint32_t);
    void reap();

    inline int relevant_timeout(const FlowInfo *, const Map &) const;
#if CLICK_USERLEVEL
    void stat_new_flow_hook(const Packet *, FlowInfo *);
#endif
    inline void packet_emit_hook(const Packet *, const click_ip *, FlowInfo *);
    inline void delete_flowinfo(const HostPair &, FlowInfo *, bool really_delete = true);
    void emit_fragment_head(HostPairInfo *hpinfo);
    FlowInfo *find_flow_info(Map &, HostPairInfo *, uint32_t ports, bool flipped, const Packet *);

    FlowInfo *uncommon_case(FlowInfo *finfo, const click_ip *iph);

    enum { ACT_EMIT, ACT_DROP, ACT_NONE };
    int handle_fragment(Packet *, HostPairInfo *);
    int handle_packet(Packet *);

    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

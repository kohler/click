// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEIPFLOWS_HH
#define CLICK_AGGREGATEIPFLOWS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/bighashmap.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateIPFlows([I<KEYWORDS>])

=s

sets aggregate annotation based on flow

=d

AggregateIPFlows monitors TCP and UDP flows, setting the aggregate annotation
on every passing packet to a flow number, and the paint annotation to a
direction indication. Non-TCP and UDP packets, second and subsequent
fragments, and short packets are emitted on output 1, or dropped if there is
no output 1.

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

AggregateIPFlows can optionally apply aggregate annotations to ICMP errors. See
the ICMP keyword argument below.

When used as a push element, AggregateIPFlows may have a second input. Packets
arriving on this input are assigned aggregate annotations just like usual.
However, these packets never generate a new aggregate annotation; if a packet
arriving on input 1 has a flow ID that has never been seen before, it will be
treated like a short packet and emitted on output 1 or dropped. You might send
ICMP errors to this input, for example.

Keywords are:

=over 8

=item TCP_TIMEOUT

The timeout for active TCP flows, in seconds. Default is 24 hours.

=item TCP_DONE_TIMEOUT

The timeout for completed TCP flows, in seconds. A completed TCP flow has seen
FIN flags on both subflows. Default is 30 seconds.

=item UDP_TIMEOUT

The timeout for UDP connections, in seconds. Default is 1 minute.

=item REAP

The garbage collection interval. Default is 10 minutes of packet time.

=item ICMP

Boolean. If true, then mark ICMP errors relating to a connection with an
aggregate annotation corresponding to that connection. ICMP error packets get
paint annotations equal to 2 plus the paint color of the encapsulated packet.
Default is false.

=item FLOWINFO

Filename. If provided, output information about each flow to that filename.

=back

AggregateIPFlows is an AggregateNotifier, so AggregateListeners can request
notifications when new aggregates are created and old ones are deleted.

=e

This configuration counts with AggregateCounter the number of packets in each
flow in a trace (not counting fragments).

   FromDump(tracefile.dump, STOP true, FORCE_IP true)
       -> AggregateIPFlows
       -> AggregateCounter
       -> Discard;

=h frozen read/write

A Boolean value indicating whether the AggregateIPFlows is frozen. When it is
frozen, no new aggregate annotations are generated -- all packets act like
they arrived on input 1. Starts out false.

=a

AggregateIP, AggregateCounter */

class AggregateIPFlows : public Element, public AggregateNotifier { public:

    AggregateIPFlows();
    ~AggregateIPFlows();

    const char *class_name() const	{ return "AggregateIPFlows"; }
    void *cast(const char *);
    AggregateIPFlows *clone() const	{ return new AggregateIPFlows; }

    void notify_noutputs(int);
    const char *processing() const	{ return "a/ah"; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();
    void cleanup(CleanupStage);

    bool handle_packet(Packet *, bool);
    void push(int, Packet *);
    Packet *pull(int);

  private:

    struct FlowInfo {
	uint32_t _aggregate;
	union {
	    unsigned active_sec;
	    FlowInfo *other;
	    void *thunk;
	} uu;
	unsigned flow_over : 2;
	bool _reverse : 1;
	FlowInfo() : _aggregate(0), flow_over(0), _reverse(0) { }
	uint32_t aggregate() const { return _aggregate; }
	bool fresh() const	{ return !_aggregate && !_reverse; }
	bool reverse() const	{ return _reverse; }
    };

    typedef BigHashMap<IPFlowID, FlowInfo> Map;
    Map _tcp_map;
    Map _udp_map;
    
    uint32_t _next;
    unsigned _active_sec;
    unsigned _gc_sec;

    unsigned _tcp_timeout;
    unsigned _tcp_done_timeout;
    unsigned _udp_timeout;
    unsigned _smallest_timeout;
    unsigned _gc_interval;

    bool _handle_icmp_errors : 1;
    bool _frozen : 1;

    FILE *_flowinfo_file;
    String _flowinfo_filename;
    
    void clean_map(Map &, uint32_t, uint32_t);
    void reap();
    const click_ip *icmp_encapsulated_header(const Packet *) const;

    enum { H_FROZEN };
    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler*);
    
};

CLICK_ENDDECLS
#endif

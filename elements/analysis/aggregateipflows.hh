// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEFLOWS_HH
#define CLICK_AGGREGATEFLOWS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/bighashmap.hh>

/*
=c

AggregateFlows([I<KEYWORDS>])

=s

sets aggregate annotation based on flow

=d

AggregateFlows monitors TCP and UDP flows, setting the aggregate annotation on
every passing packet to a flow number, and the paint annotation to a direction
indication. Non-TCP and UDP packets, second and subsequent fragments, and
short packets are emitted on output 1, or dropped if there is no output 1.

AggregateFlows uses source and destination addresses and source and
destination ports to distinguish flows. Reply packets get the same flow
number, but a different paint annotation. Old flows die after a configurable
timeout, after which new packets with the same addresses and ports get a new
flow number. UDP, active TCP, and completed TCP flows have different timeouts.

Flow numbers are assigned sequentially, starting from 1. Different flows get
different numbers. Paint annotations are set to 0 or 1, depending on whether
packets are on the forward or reverse subflow. (The first packet seen on each
flow gets paint color 0; reply packets get paint color 1.)

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

=back

=a

AggregateIP, AggregateCounter */

class AggregateFlows : public Element { public:

    AggregateFlows();
    ~AggregateFlows();

    const char *class_name() const	{ return "AggregateFlows"; }
    AggregateFlows *clone() const	{ return new AggregateFlows; }

    void notify_noutputs(int);
    const char *processing() const	{ return "a/ah"; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    Packet *simple_action(Packet *);
    
  private:

    struct FlowInfo {
	uint32_t _aggregate;
	union {
	    unsigned active_sec;
	    FlowInfo *other;
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

    void clean_map(Map &, uint32_t, uint32_t);
    void reap();
    
};

#endif

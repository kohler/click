// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipflowrawsockets.{cc,hh} -- creates sockets for TCP/UDP flows
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 *
 * $Id: ipflowrawsockets.hh,v 1.2 2004/04/27 17:03:06 eddietwo Exp $
 */

#ifndef CLICK_IPFLOWRAWSOCKETS_HH
#define CLICK_IPFLOWRAWSOCKETS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "../analysis/aggregatenotifier.hh"
extern "C" {
#include <pcap.h>
}
CLICK_DECLS

/*
=c

IPFlowRawSockets([I<KEYWORDS>])

=s analysis

creates separate sockets for each TCP/UDP flow

=d

Sends and receives IP packets via raw sockets, one socket per flow. It
distinguishes flows by their aggregate annotations. You usually will
run IPFlowRawSockets downstream of an AggregateIPFlows element.

On some systems, packets larger than SNAPLEN will be truncated;
default SNAPLEN is 2046 bytes.

On PlanetLab Linux, safe raw sockets are opened
E<lparen>http://www.planet-lab.org/raw_sockets/). Safe raw sockets bypass the
kernel stack, so no additional firewalling is necessary.

On regular Linux, you will need to firewall the source ports that you
expect IPFlowRawSockets to use so that the kernel does not attempt to
answer for the raw connections that may be established by upstream
elements. For example, in a NAPT configuration, you might firewall TCP
and UDP ports 50000-65535 with the iptables command:

iptables -A INPUT -p tcp --dport 50000:65535 -j DROP
iptables -A INPUT -p udp --dport 50000:65535 -j DROP

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then IPFlowRawSockets will ask the element for notification when flows are
deleted. It uses that notification to free its state early. It's a very good
idea to supply a NOTIFIER.

An AggregateNotifier in the data path is mandatory anyway. See below
for an example usage of this element.

=item SNAPLEN

Unsigned integer. Maximum receive packet length. This value represents
the MRU of the IPFlowSocket. Packets larger than SNAPLEN will be
truncated.

=back

=n

Only available in user-level processes.

=e

The following snippet is the heart of a basic user-level NAPT
configuration with an external address of 10.0.0.1 and an internal IP path
represented by ip_from_intern and ip_to_intern.

af :: AggregateIPFlows(TRACEINFO -)

cp :: CheckPaint(0)

IPRewriterPatterns(to_world_pat 10.0.0.1 50000-65535 - -)

rw :: IPRewriter(
	pattern to_world_pat 0 1,
	drop
)

socket :: IPFlowRawSockets(NOTIFIER af)

// Forward direction
ip_from_intern -> af
af -> cp
cp[0] -> [0]rw
rw[0] -> GetIPAddress -> CheckIPHeader -> socket

// Reverse direction
socket -> CheckIPHeader -> IPClassifier(tcp or udp) -> [1]rw
rw[1] -> af
cp[1] -> ip_to_intern

=a

ToIPFlowDumps, AggregateIPFlows */

class IPFlowRawSockets : public Element, public AggregateListener { public:
  
    IPFlowRawSockets();
    ~IPFlowRawSockets();
  
    const char *class_name() const	{ return "IPFlowRawSockets"; }
    const char *processing() const	{ return "a/h"; }
    const char *flow_code() const	{ return "x/y"; }
    IPFlowRawSockets *clone() const	{ return new IPFlowRawSockets; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void push(int, Packet *);
    bool run_task();

    void aggregate_notify(uint32_t, AggregateEvent, const Packet *);

  private:

    class Flow { public:

	Flow(const Packet *);
	~Flow();

	int initialize(ErrorHandler *, int snaplen);

	uint32_t aggregate() const	{ return _aggregate; }
	Flow *next() const		{ return _next; }
	void set_next(Flow *f)		{ _next = f; }
	int rd()			{ return _rd; }
	pcap_t *pcap()			{ return _pcap; }
	int datalink()			{ return _datalink; }

	void send_pkt(Packet *, ErrorHandler *);

      private:

	enum { NPKT = 128, NNOTE = 32 };
	
	Flow *_next;
	IPFlowID _flowid;
	int _ip_p;
	uint32_t _aggregate;
	int _wd, _rd;
	pcap_t *_pcap;
	int _datalink;
	
    };

    enum { FLOWMAP_BITS = 10, NFLOWMAP = 1 << FLOWMAP_BITS };
    Flow *_flowmap[NFLOWMAP];
    Vector<Flow *> _flows;
    int _nflows;

    uint32_t _nnoagg;
    uint32_t _nagg;
    AggregateNotifier *_agg_notifier;

    Task _task;
    NotifierSignal _signal;

    Timer _gc_timer;
    Vector<uint32_t> _gc_aggs;

    int _snaplen;

    Flow *find_aggregate(uint32_t, const Packet * = 0);
    void end_flow(Flow *, ErrorHandler *);
    void selected(int);
    static void gc_hook(Timer *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler*);

};

CLICK_ENDDECLS
#endif

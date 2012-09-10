// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEIPADDRPAIR_HH
#define CLICK_AGGREGATEIPADDRPAIR_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

AggregateIPAddrPair(I<KEYWORDS>)

=s ipmeasure

sets aggregate annotation based on IP source/destination address pair

=d

AggregateIPAddrPair sets the aggregate annotation on every passing packet to a
flow number based on its IP address pair.  Packets with the same
source-destination address pair are assigned the same aggregate number, as are
reply packets (with source and destination addresses switched).  The paint
annotation is set to 0 or 1 to indicate which direction the packet was headed.

Flow numbers are assigned sequentially, starting from 1. Different flows get
different numbers. Paint annotations are set to 0 or 1, depending on whether
packets are on the forward or reverse subflow. (The first packet seen on each
flow gets paint color 0; reply packets get paint color 1.)

Keywords are:

=over 8

=item TIMEOUT

The timeout for active address pairs, in seconds.  Default is no timeout
(address pairs never expire).  If a timeout is specified, then information
about an address pair is forgotten after that timeout passes; if the address
pair occurs again later, it will get a new aggregate annotation.

=item REAP

The garbage collection interval. Default is 20 minutes of packet time.

=back

AggregateIPAddrPair is an AggregateNotifier, so AggregateListeners can request
notifications when new aggregates are created and old ones are deleted.

=h clear write-only

Clears all flow information.  Future packets will get new aggregate annotation
values.

=a

AggregateIPFlows, AggregateCounter, AggregateIP
*/

class AggregateIPAddrPair : public Element, public AggregateNotifier { public:

    AggregateIPAddrPair() CLICK_COLD;
    ~AggregateIPAddrPair() CLICK_COLD;

    const char *class_name() const	{ return "AggregateIPAddrPair"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

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
	Timestamp last_timestamp;
	uint32_t aggregate;
	bool reverse;
	FlowInfo()		: aggregate(0) { }
    };

    typedef HashTable<HostPair, FlowInfo> Map;
    Map _map;

    unsigned _active_sec;
    unsigned _gc_sec;

    uint32_t _timeout;
    uint32_t _gc_interval;
    bool _timestamp_warning;
    uint32_t _next;

    void reap();

    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

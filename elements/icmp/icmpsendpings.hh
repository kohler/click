// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ICMPSENDPINGS_HH
#define CLICK_ICMPSENDPINGS_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
=c

ICMPPingSource(SADDR, DADDR [, I<keywords> INTERVAL, IDENTIFIER, LIMIT, DATA,
ACTIVE])

=s ICMP, sources

periodically sends ICMP echo requests

=d

Periodically emits ping packets with source IP address SRC and destination
address DST. Advances the "sequence" field by one each time. (The sequence
field is stored in network byte order in the packet.)

Keyword arguments are:

=over 8

=item INTERVAL

Amount of time between pings, in seconds. Default is 1.

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted pings. Default is
0.

=item LIMIT

Integer. The number of pings to send; but if LIMIT is negative, sends pings
forever. Default is -1.

=item DATA

String. Extra data in emitted pings. Default is the empty string (nothing).

=item ACTIVE

Boolean.  Whether ICMPPingSource is active.  Default is true.

=back

=a

ICMPPingEncap, ICMPPingResponder, ICMPPingRewriter */

class ICMPPingSource : public Element { public:
  
    ICMPPingSource();
    ~ICMPPingSource();
  
    const char *class_name() const		{ return "ICMPPingSource"; }
    const char *processing() const		{ return PUSH; }
    void notify_ninputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
  
    void run_timer();
    void push(int, Packet *);
  
  private:
  
    struct in_addr _src;
    struct in_addr _dst;
    int _count;
    int _limit;
    uint16_t _icmp_id;
    int _interval;
    Timer _timer;
    String _data;
    bool _active;

#ifdef HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    struct ReceiverInfo {
	int nreceived;
	int nduplicate;
	uint32_t time_min;
	uint32_t time_max;
	counter_t time_sum;
	counter_t time_sq_sum;
	struct timeval send_timestamp[65536];
    };
    ReceiverInfo *_receiver;

    static int write_handler(const String&, Element*, void*, ErrorHandler*);
    
};

CLICK_ENDDECLS
#endif

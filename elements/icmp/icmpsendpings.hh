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

ICMPPingSource's optional input accepts replies to the pings it sends.  If you
send replies to this input, ICMPPingSource will print reply reports and keep
loss and RTT statistics like the ping(1) program.  You can access those
stistics with the "C<summary>" handler.

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

=item VERBOSE

Boolean.  Whether ICMPPingSource should print reports when echo replies
arrive.  Default is true.

=back

=h active read/write

Returns or sets the ACTIVE argument.

=h count read-only

Reports the number of packets sent so far.

=h limit write-only

Sets the LIMIT argument.

=h interval write-only

Sets the INTERVAL argument.

=h reset_counts write-only

Resets all counters to zero.

=h summary read-only

Returns ping(1)-style summary information: number of packets sent and
received, loss rate, and RTT statistics.  Only available if ICMPPingSource had
an input.

=h rtt_min read-only

Returns the minimum RTT observed, or 0 if no RTTs have been observed.  Only
available if ICMPPingSource had an input.

=h rtt_max read-only

Returns the maximum RTT observed, or 0 if no RTTs have been observed.  Only
available if ICMPPingSource had an input.

=h rtt_avg read-only

Returns the average RTT observed, or 0 if no RTTs have been observed.  Only
available if ICMPPingSource had an input.

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
    bool _verbose;

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

    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);
    
};

CLICK_ENDDECLS
#endif

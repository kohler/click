// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ICMPSENDPINGS_HH
#define CLICK_ICMPSENDPINGS_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
=c

ICMPPingSource(SRC, DST [, I<keywords> INTERVAL, IDENTIFIER, LIMIT, DATA, ACTIVE])

=s icmp

periodically sends ICMP echo requests

=d

Periodically emits ping packets with source IP address SRC and destination
address DST.  Advances the "sequence" field by one each time.  (The sequence
field is stored in network byte order in the packet.)  When its output is
pull, generates a ping packet on every pull.

ICMPPingSource's optional input accepts replies to the pings it sends.  If you
send replies to this input, ICMPPingSource will print reply reports and keep
loss and RTT statistics like the ping(1) program.  You can access those
stistics with the "C<summary>" handler.

Keyword arguments are:

=over 8

=item INTERVAL

Amount of time between pings, in seconds. Defaults to 1.

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted pings. Defaults to
0.

=item LIMIT

Integer. The number of pings to send; but if LIMIT is negative, sends pings
forever. Defaults to -1.

=item STOP

Boolean. If true, stop the router when LIMIT is reached. Defaults to false.

=item DATA

String. Extra data in emitted pings. Defaults to the empty string (nothing).

=item ACTIVE

Boolean. Whether ICMPPingSource is active. Defaults to true.

=item MIRROR

Boolean. If true, then when ICMPPingSource receives an echo reply, it
generates the next echo request immediately, using the same packet data.
Overrides LIMIT; only available if output is push. Defaults to false.

=item VERBOSE

Boolean. Whether ICMPPingSource should print reports when echo replies
arrive. Defaults to true.

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

=h src read/write

Returns or sets the SRC argument.

=h dst read/write

Returns or sets the DST argument.

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
    const char *port_count() const		{ return "0-1/1"; }
    const char *processing() const		{ return "h/a"; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void run_timer(Timer *);
    void push(int, Packet *);
    Packet* pull(int);

  private:

    struct in_addr _src;
    struct in_addr _dst;
    int _count;
    int _limit;
    uint16_t _icmp_id;
    uint32_t _interval;
    Timer _timer;
    String _data;
    bool _active;
    bool _verbose;
    bool _stop;
    bool _mirror;

#if HAVE_INT64_TYPES && !CLICK_LINUXMODULE
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
	Timestamp send_timestamp[65536];
    };
    ReceiverInfo *_receiver;

    Packet* make_packet(WritablePacket *q);
    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);

};

CLICK_ENDDECLS
#endif

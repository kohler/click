// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEIPADDRPAIR_HH
#define CLICK_AGGREGATEIPADDRPAIR_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AggregateIPAddrPair(I<KEYWORDS>)

=s analysis, IP

sets aggregate annotation based on IP packet field

=d

AggregateIP sets the aggregate annotation on every passing packet to a portion
of that packet's IP header, transport header, or payload, depending on the
value of the FIELD argument.

FIELD can be the name of a header field, like C<"ip tos">, or a general
offset-length specification, like C<"ip[8, 2]">. Either form can be modified
with a mask, such as C<"ip src/8"> or C<"ip[8, 2] & 0x3F0">. (Note that the
offset-length form contains a comma, which you must protect with single or
double quotes.)

The aggregate annotation value uses host byte order.

General offset-length specifications begin with C<"ip">, C<"transp">, or
C<"data">, depending on whether the offset should be measured relative to the
IP header, transport header, or payload. (The names C<"tcp">, C<"udp">, and
C<"icmp"> act like C<"transp">, but enforce the specified IP protocol.) Next
comes the offset and length, which can take several forms:

=over 8

=item C<[OFFSET, LENGTH]>

The LENGTH bytes starting at byte OFFSET.

=item C<[OFFSET1-OFFSET2]>

From byte OFFSET1 to byte OFFSET2, inclusive.

=item C<[OFFSET]>

The single byte at OFFSET.

=item C<{OFFSET, LENGTH}>, C<{OFFSET1-OFFSET2}>, C<{OFFSET}>

Similar, but OFFSETs and LENGTHs are measured in bits.

=back

Finally, the mask can equal either `C</NUM>', which means take the top NUM
bits of the field, or `C<& MASK>', which means bitwise-and the field with
MASK. (MASK must contain exactly one set of contiguous 1 bits.)

Keyword arguments are:

=over 8

=item INCREMENTAL

Boolean. If true, then incrementally update the aggregate annotation: given a
field N bits wide with value V, and an old aggregate annotation of O, the new
aggregate annotation will equal (O * 2^N) + V. Default is false.

=item UNSHIFT_IP_ADDR

Boolean. If true, and the aggregated field lies within either the IP source or
destination address, then set the aggregate annotation to the masked portion
of that address without shifting. For example, consider a packet with source
address 1.0.0.0, and aggregate field C<"ip src/8">. Without UNSHIFT_IP_ADDR,
the packet will get aggregate annotation 1; with UNSHIFT_IP_ADDR, it will get
aggregate annotation 16777216. Default is false.

=back

=n

Packets lacking the specified field are pushed onto output 1, or dropped if
there is no output 1. A packet may lack a field because it is too short, it is
a fragment, or it has the wrong protocol. (C<"tcp sport">, for example, is
relevant only for first-fragment TCP packets; C<"data"> specifications work
only for first-fragment TCP and UDP.)

The simple specifications C<"sport"> and C<"dport"> (no C<"ip">, C<"tcp">, or
C<"udp">) apply to either TCP or UDP packets.

=e

Here are a bunch of equivalent ways to ask for the top 8 bits of the IP source
address:

	AggregateIP(ip src/8)
	AggregateIP(ip src & 0xFF000000)
	AggregateIP(ip[12])
	AggregateIP("ip[12, 1]")	// protect comma
	AggregateIP("ip{96, 8}")
	AggregateIP(ip{96-103})

=h header read-only

Returns the header type AggregateIP is using: either "ip", "transp", or
"payload".

=h bit_offset read-only

Returns the offset into the header of the start of the aggregated field, in
bits.

=h bit_length read-only

Returns the length of the aggregated field, in bits.

=a

AggregateLength, AggregateFlows, AggregateCounter

*/

class AggregateIPAddrPair : public Element { public:

    AggregateIPAddrPair();
    ~AggregateIPAddrPair();

    const char *class_name() const	{ return "AggregateIPAddrPair"; }

    void notify_noutputs(int);
    const char *processing() const	{ return "a/ah"; }
    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

    struct HostPair {
	uint32_t a;
	uint32_t b;
	HostPair() : a(0), b(0) { }
	HostPair(uint32_t aa, uint32_t bb) : a(aa), b(bb) { if (a > b) a ^= b ^= a ^= b; }
    };
    
  private:

    struct FlowInfo {
	Timestamp last_timestamp;
	uint32_t aggregate;
	FlowInfo()		: aggregate(0) { }
    };
    
    typedef HashMap<HostPair, FlowInfo> Map;
    Map _map;

    unsigned _active_sec;
    unsigned _gc_sec;
    
    int _timeout;
    unsigned _gc_interval;
    bool _timestamp_warning;
    uint32_t _next;

    Packet *handle_packet(Packet *);
    Packet *bad_packet(Packet *);

    static String read_handler(Element *, void *);
    
};

CLICK_ENDDECLS
#endif

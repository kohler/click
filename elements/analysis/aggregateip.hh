// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEIP_HH
#define CLICK_AGGREGATEIP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AggregateIP(FIELD, I<KEYWORDS>)

=s

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

Valid header field names are C<"ip vers">, C<"ip hl">, C<"ip tos">, C<"ip
dscp">, C<"ip ecn">, C<"ip len">, C<"ip id">, C<"ip off"> (the entire
fragmentation offset field), C<"ip rf">, C<"ip df">, C<"ip mf">, C<"ip
fragoff"> (just the fragmentation offset value, without the RF, DF, and MF
bits), C<"ip ttl">, C<"ip proto">, C<"ip sum">, C<"ip src">, C<"ip dst">,
C<"udp sport">, C<"udp dport">, C<"udp len">, C<"udp sum">, C<"tcp sport">,
C<"tcp dport">, C<"tcp seqno">, C<"tcp ackno">, C<"tcp hl">, C<"tcp flags">,
C<"tcp fin">, C<"tcp syn">, C<"tcp rst">, C<"tcp psh">, C<"tcp ack">, C<"tcp
urg">, C<"tcp win">, C<"tcp sum">, C<"tcp urp">, C<"icmp type">, C<"icmp
code">, and C<"icmp sum">.

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

=a

AggregateLength, AggregateFlows, AggregateCounter

*/

class AggregateIP : public Element { public:

    AggregateIP();
    ~AggregateIP();

    const char *class_name() const	{ return "AggregateIP"; }
    AggregateIP *clone() const		{ return new AggregateIP; }

    void notify_noutputs(int);
    const char *processing() const	{ return "a/ah"; }
    int configure(Vector<String> &, ErrorHandler *);

    void push(int, Packet *);
    Packet *pull(int);
    
    enum {
	AG_NONE = 0, AG_IP = 1, AG_TRANSP = 2, AG_PAYLOAD = 3
    };
    
    class Field { public:

	Field()			: _type(0), _offset(0), _length(0) { }
	Field(uint32_t type, uint32_t offset, uint32_t length);
	Field(uint32_t type, uint32_t proto, uint32_t offset, uint32_t length);

	uint32_t type() const	{ return _type; }
	uint32_t proto() const	{ return _proto; }
	uint32_t offset() const	{ return _offset; }
	uint32_t length() const	{ return _length; }
	
	const char *unparse_type() const;
	void unparse(StringAccum &) const;
	String unparse() const;

	int parse(const String &, ErrorHandler *);
	
      private:
	
	uint32_t _type : 8;
	uint32_t _proto : 24;
	uint32_t _offset;
	uint32_t _length;

	static void initialize_wordmap();
	int add_word(String, ErrorHandler *);
	int apply_mask(uint32_t, const char *, ErrorHandler *);
	
    };

    struct ChunkMap;
    
  private:

    uint32_t _offset;
    uint32_t _shift;
    uint32_t _mask;
    bool _incremental;
    Field _f;

    Packet *handle_packet(Packet *);
    Packet *bad_packet(Packet *);
    
};


inline
AggregateIP::Field::Field(uint32_t type, uint32_t offset, uint32_t length)
    : _type(type), _offset(offset), _length(length)
{
    assert(_type != AG_TRANSP);
}

inline
AggregateIP::Field::Field(uint32_t type, uint32_t proto, uint32_t offset, uint32_t length)
    : _type(type), _proto(proto), _offset(offset), _length(length)
{
    assert(_type == AG_TRANSP);
}

CLICK_ENDDECLS
#endif

#ifndef CLICK_AGGCOUNTER_HH
#define CLICK_AGGCOUNTER_HH
#include <click/element.hh>

/*
=c

AggregateCounter([I<KEYWORDS>])

=s measurement

maintains information about aggregate annotations

=d

AggregateCounter maintains per-aggregate counts of how many packets or bytes
it has seen. Each aggregate annotation value it sees gets a different value.
Call its C<write_file> or C<write_ascii_file> write handler to get a dump of
the information.

Keyword arguments are:

=over 8

=item BYTES

Boolean. If true, then count bytes seen, not packets seen. Default is false.

=item MULTIPACKET

Boolean. If true, and BYTES is false, then use packets' packet count
annotations to add to the number of packets seen. Elements like
FromIPSummaryDump set this annotation. Default is true.

=item EXTRA_LENGTH

Boolean. If true, and BYTES is true, then use packets' extra length
annotations to add to the length counted. Elements like FromDump set this
annotation. Default is true.

=back

=h write_file write-only

Argument is a filename, or `C<->', meaning standard out. Write a packed binary
file containing all current data to the specified filename. The format is a
couple ASCII lines, followed by a line containing `C<$packed>', followed by N
8-byte records. In each record, bytes 1-4 are the aggregate, and bytes 5-8 are
the count. Both values are 32-bit integers in host byte order.

=h write_ascii_file write-only

Argument is a filename, or `C<->', meaning standard out. Write an ASCII file
containing all current data to the specified filename. The format is a couple
ASCII lines, followed by N data lines, each containing the aggregate ID in
decimal, a space, then the count in decimal.

=n

The aggregate identifier is stored in host byte order. Thus, the aggregate ID
corresponding to IP address 128.0.0.0 equals 2147483648.

Only available in user-level processes.

=e

This configuration reads an IP summary dump in from standard input, aggregates
based on destination IP address, and counts packets. When the dump is done,
Click will write the aggregate counter's data to standard output, in ASCII
form.

  FromIPSummaryDump(-, STOP true)
	-> AggregateIP(ip dst)
	-> ac :: AggregateCounter
	-> Discard;

  DriverManager(wait_pause,
	write ac.write_ascii_file -);

=a

AggregateIP, FromIPSummaryDump, FromDump, tcpdpriv(1) */

class AggregateCounter : public Element { public:
  
    AggregateCounter();
    ~AggregateCounter();
  
    const char *class_name() const	{ return "AggregateCounter"; }
    const char *processing() const	{ return AGNOSTIC; }
    AggregateCounter *clone() const	{ return new AggregateCounter; }

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    void add_handlers();

    Packet *simple_action(Packet *);

    int write_file(String, bool, ErrorHandler *) const;
    
  private:

    struct Node {
	uint32_t aggregate;
	uint32_t count;
	Node *child[2];
    };

    bool _bytes : 1;
    bool _packet_count : 1;
    bool _extra_length : 1;
    
    Node *_root;
    Node *_free;
    Vector<Node *> _blocks;

    Node *new_node();
    Node *new_node_block();
    void free_node(Node *);

    Node *make_peer(uint32_t, Node *);
    Node *find_node(uint32_t);

    static uint32_t write_nodes(Node *, FILE *, bool, uint32_t *, int &, int, ErrorHandler *);
    static int write_file_handler(const String &, Element *, void *, ErrorHandler *);
    
};

inline AggregateCounter::Node *
AggregateCounter::new_node()
{
    if (_free) {
	Node *n = _free;
	_free = n->child[0];
	return n;
    } else
	return new_node_block();
}

inline void
AggregateCounter::free_node(Node *n)
{
    n->child[0] = _free;
    _free = n;
}

#endif

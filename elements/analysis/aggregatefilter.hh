#ifndef CLICK_AGGREGATEFILTER_HH
#define CLICK_AGGREGATEFILTER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AggregateFilter([I<KEYWORDS>])

=s measurement

maintains information about aggregate annotations

=d

AggregateCounter maintains counts of how many packets or bytes it has seen for
each aggregate value. Each aggregate annotation value gets a different count.
Call its C<write_file> or C<write_ascii_file> write handler to get a dump of
the information.

The C<freeze> handler, and the C<AGGREGATE_FREEZE> and C<COUNT_FREEZE>
keyword arguments, can put AggregateCounter in a frozen state. Frozen
AggregateCounters only update existing counters; they do not create new
counters for previously unseen aggregate values.

AggregateCounter may have one or two inputs. The optional second input is
always frozen. (It is only useful when the element is push.) It may also have
two outputs. If so, and the element is push, then packets that were counted
are emitted on the first output, while other packets are emitted on the second
output.

Keyword arguments are:

=over 8

=item BYTES

Boolean. If true, then count bytes, not packets. Default is false.

=item IP_BYTES

Boolean. If true, then do not count bytes from the link header. Default is
false.

=item MULTIPACKET

Boolean. If true, and BYTES is false, then use packets' packet count
annotations to add to the number of packets seen. Elements like
FromIPSummaryDump set this annotation. Default is true.

=item EXTRA_LENGTH

Boolean. If true, and BYTES is true, then include packets' extra length
annotations in the byte counts. Elements like FromDump set this annotation.
Default is true.

=item AGGREGATE_STOP

Unsigned. Stop the router once I<N> distinct aggregates have been seen.
Default is never to stop.

=item AGGREGATE_FREEZE

Unsigned. Freeze the AggregateCounter once I<N> distinct aggregates have been
seen. Default is never to freeze.

=item AGGREGATE_CALL

Argument is `I<N> I<HANDLER> [I<VALUE>]'. Call the given write handler, with
the supplied value, once I<N> distinct aggregates have been seen.

The three AGGREGATE keywords are mutually exclusive. Supply at most one of
them.

=item COUNT_STOP

Unsigned. Stop the router once the total count (of bytes or packets) has
reached or exceeded I<N>. Default is never to stop.

=item COUNT_FREEZE

Unsigned. Freeze the AggregateCounter once the total count has reached or
exceeded I<N>. Default is never to freeze.

=item COUNT_CALL

Argument is `I<N> I<HANDLER> [I<VALUE>]'. Call the given write handler, with
the supplied value, once the total count has reached or exceeded I<N>.

The three COUNT keywords are mutually exclusive. Supply at most one of
them.

=item BANNER

String. This banner is written to the head of any output file. It should
probably begin with a comment character, like `!' or `#'. Default is empty.

=back

=h write_file write-only

Argument is a filename, or `C<->', meaning standard out. Write a packed binary
file containing all current data to the specified filename. The format is a
couple ASCII lines, followed by a line containing `C<$packed_le>' or
`C<$packed_be>', followed by N 8-byte records. In each record, bytes 1-4 are
the aggregate, and bytes 5-8 are the count. Both values are 32-bit integers.
The byte order is indicated by the `C<$packed>' line: `C<$packed_le>' means
little-endian, `C<$packed_be>' means big-endian.

=h write_ascii_file write-only

Argument is a filename, or `C<->', meaning standard out. Write an ASCII file
containing all current data to the specified filename. The format is a couple
ASCII lines, followed by N data lines, each containing the aggregate ID in
decimal, a space, then the count in decimal.

=h write_ip_file write-only

Argument is a filename, or `C<->', meaning standard out. Write an ASCII file
containing all current data to the specified filename. The format is as in
C<write_ascii_file>, except that aggregate IDs are printed as IP addresses.

=h freeze read/write

Returns or sets the AggregateCounter's frozen state, which is `true' or
`false'. AggregateCounter starts off unfrozen.

=h active read/write

Returns or sets the AggregateCounter's active state. When AggregateCounter is
inactive (`false'), it does not record information about any packets that
pass. It starts out active.

=h stop write-only

When any value is written to this handler, AggregateCounter sets `active' to
false and additionally stops the driver.

=h reaggregate_counts write-only

When any value is written to this handler, AggregateCounter will recalculate
its counters. The new aggregate identifiers equal the old counts; the new
counts represent how many times each old count appeared. The old aggregate
identifiers are thrown away. To put it another way, AggregateCounter creates a
multiset containing all aggregate counts, then stores each count as an
aggregate, with its number of occurrences in the multiset as its count.

=h banner read/write

Returns or sets the BANNER setting.

=h aggregate_call read/write

Returns or sets the AGGREGATE_CALL setting.

=h count_call read/write

Returns or sets the COUNT_CALL setting.

=h nagg read-only

Returns the number of aggregates that have been seen so far.

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

class AggregateFilter : public Element { public:
  
    AggregateFilter();
    ~AggregateFilter();
  
    const char *class_name() const	{ return "AggregateFilter"; }
    const char *processing() const	{ return PUSH; }
    AggregateFilter *clone() const	{ return new AggregateFilter; }

    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    void cleanup(CleanupStage);

    void push(int, Packet *);

    enum { GROUPSHIFT = 8, GROUPMASK = 0xFFFFFFFFU << GROUPSHIFT,
	   NINGROUP = 1 << GROUPSHIFT, INGROUPMASK = NINGROUP - 1,
	   NBUCKETS = 256 };
    
  private:

    struct Group {
	uint32_t groupno;
	Group *next;
	uint8_t filters[NINGROUP];
	Group(uint32_t);
    };

    Group *_groups[NBUCKETS];
    int _default_output;

    Group *find_group(uint32_t);
    
};

#endif

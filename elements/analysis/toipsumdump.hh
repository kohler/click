// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TOIPSUMDUMP_HH
#define CLICK_TOIPSUMDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/straccum.hh>
#include <click/notifier.hh>
#include "ipsumdumpinfo.hh"
CLICK_DECLS

/*
=c

ToIPSummaryDump(FILENAME [, I<keywords>])

=s traces

writes packet summary information to an ASCII file

=d

Writes summary information about incoming packets to FILENAME in a simple
ASCII format---each line corresponds to a packet.  The FIELDS keyword
argument determines what information is written.  Writes to standard output if
FILENAME is a single dash `C<->'.  The BINARY keyword argument writes a packed
binary format to save space.

ToIPSummaryDump uses packets' extra-length and extra-packet-count annotations.

ToIPSummaryDump can optionally be used as a filter: it pushes received packets
to its output if that output exists.

Keyword arguments are:

=over 8

=item FIELDS

Space-separated list of field names. Each line of the summary dump will
contain those fields. Valid field names, with examples, are:

   timestamp    Packet timestamp: '996033261.451094' (either
		microsecond or nanosecond precision, depending
		on how Click was compiled)
   ts_sec       Seconds portion of timestamp: '996033261'
   ts_usec      Microseconds portion of timestamp: '451094'
   ts_usec1     Packet timestamp in microseconds
                (ts_sec*1e6 + ts_usec): '996033261451094'
   ip_src       IP source address: '192.150.187.37'
   ip_dst       IP destination address: '192.168.1.100'
   ip_frag      IP fragment: 'F' (1st frag), 'f' (2nd or
                later frag), '!' (nonfrag with DF), or
		'.' (normal nonfrag)
   ip_fragoff   IP fragmentation offset: '0', '0+', '0!'
                ('+' adds MF, '!' adds DF; offset in bytes)
   ip_len       IP length: '132'
   ip_proto     IP protocol: '10', or 'I' for ICMP, 'T' for
                TCP, 'U' for UDP
   ip_hl        IP header length in bytes: '20'
   ip_id        IP ID: '48759'
   ip_tos       IP type of service: '29'
   ip_dscp      IP Differentiated Services code point: '29'
   ip_ecn       IP ECN capability: 'no', 'ect1', 'ect2', 'ce'
   ip_ttl       IP time-to-live: '254'
   ip_sum       IP checksum: '43812'
   ip_opt       IP options (see below)
   sport        TCP/UDP source port: '22'
   dport        TCP/UDP destination port: '2943'
   tcp_seq      TCP sequence number: '93167339'
   tcp_ack      TCP acknowledgement number: '93178192'
   tcp_off      TCP offset in bytes: '20'
   tcp_flags    TCP flags: 'SA', '.'
   tcp_opt      TCP options (see below)
   tcp_ntopt    TCP options except NOP, EOL and timestamp
                (see below)
   tcp_sack     TCP SACK options (see below)
   tcp_window   TCP receive window: '480'
   tcp_urp      TCP urgent pointer: '0'
   udp_len      UDP length: '34'
   icmp_type    ICMP type: '0'
   icmp_code    ICMP code: '2'
   icmp_type_name  ICMP type, named if available: 'echo',
                '200'
   icmp_code_name  ICMP code, named if available:
                'sourcequench', '0'
   icmp_flowid  ICMP flow identifier, in network order: '256'
   icmp_seq     ICMP sequence number, in network order: '0'
   icmp_nextmtu  ICMP next-hop MTU (unreach.needfrag only):
                '256'
   payload      Payload (not including IP/TCP/UDP headers,
                for this fragment), in a string
   payload_len  Payload length: '34'
   payload_md5  Payload MD5 checksum (in ASCII output,
                expressed using 22 chars from [A-Za-z0-9_@])
   payload_md5_hex  Payload MD5 checksum (in ASCII output,
                expressed using 32 hexadecimal digits)
   ip_capture_len  Portion of IP length that contains
                actual packet data (as opposed to the extra
		length annotation): '34'
   count        Number of packets: '1'
   direction    Link number (PAINT_ANNO): '2', or '>'/'L'
                for paint 0, '<'/'R'/'X' for paint 1
   link, paint  Like 'direction', but always numeric
   aggregate    Aggregate number (AGGREGATE_ANNO): '973'
   first_timestamp   Packet "first timestamp" (FIRST_
                TIMESTAMP_ANNO): '996033261.451094'
   eth_src      Ethernet source: '00-0A-95-A6-D9-BC'
   eth_dst      Ethernet source: '00-0A-95-A6-D9-BC'
   wire_len     Packet wire length: '54'

If a field does not apply to a particular packet -- for example, 'C<sport>' on
an ICMP packet -- ToIPSummaryDump prints a single dash for that value.

Default FIELDS is 'ip_src ip_dst'. You may also use spaces instead of
underscores, in which case you must quote field names that contain a space --
for example, 'C<ip_src ip_dst "tcp seq">'.

=item HEADER

Boolean. If true, then print any 'C<!>' header lines at the beginning
of the dump to describe the dump format. Default is true.

=item VERBOSE

Boolean. If true, then print out a couple comments at the beginning of the
dump describing the hostname and starting time, in addition to the 'C<!data>'
line describing the dumped fields. Ignored if HEADER is false. Default is
false.

=item BANNER

String. If supplied, prints a 'C<!creator "BANNER">' comment at the beginning
of the dump. Ignored if HEADER is false.

=item BINARY

Boolean. If true, then output packet records in a binary format (explained
below). Defaults to false.

=item MULTIPACKET

Boolean. If true, and the FIELDS option doesn't contain 'C<count>', then
generate multiple summary entries for packets with nonzero extra-packets
annotations. For example, if MULTIPACKET is true, and a packet has
extra-packets annotation 1, then ToIPSummaryDump will generate 2 lines for
that packet in the dump. False by default.

=item BAD_PACKETS

Boolean. If true, then print 'C<!bad MESSAGE>' lines for packets with bad IP,
TCP, or UDP headers, as well as normal output.  The 'C<!bad>' line immediately
precedes the corresponding packet.  Output will contain dashes 'C<->' in place
of data from bad headers.  Default is false.

=item CAREFUL_TRUNC

Boolean.  If true, then print 'C<!bad truncated IP length>' lines for packets
whose data plus extra length annotation is less than their IP length.
B<Tcpdump> prints 'C<truncated-ip - N bytes missing>' for such packets.
Actual packet output immediately follows the 'C<!bad>' line.  Default is true.

=item EXTRA_LENGTH

Boolean.  If false, then ignore extra length annotations.  Defaults to true.

=back

=e

Here are a couple lines from the start of a sample verbose dump.

  !IPSummaryDump 1.3
  !creator "aciri-ipsumdump -i wvlan0"
  !host no.lcdf.org
  !runtime 996022410.322317 (Tue Jul 24 17:53:30 2001)
  !data ip_src ip_dst
  63.250.213.167 192.150.187.106
  63.250.213.167 192.150.187.106

The end of the dump may contain a comment 'C<!drops N>', meaning that C<N>
packets were dropped before they could be entered into the dump.

A 'C<!flowid>' comment can specify source and destination addresses and ports
for packets that otherwise don't have one.  Its arguments are 'C<!flowid SRC
SPORT DST DPORT [PROTO]>'.

Any packet line may contain fewer fields than specified in the 'C<!data>'
line, down to one field. Missing fields are treated as 'C<->'.

=n

The 'C<len>' and 'C<payload_len>' fields use the extra length
annotation. The 'C<count>' field uses the extra packets annotation.

The characters corresponding to TCP flags are as follows:

   Flag name  Character  Value
   ---------  ---------  -----
   FIN        F          0x01
   SYN        S          0x02
   RST        R          0x04
   PSH        P          0x08
   ACK        A          0x10
   URG        U          0x20
   ECE        E          0x40
   CWR        C          0x80
   NS         N          0x100

The 'C<W>' character is also acceptable for CWR.  Old IP summary dumps might
contain an unsigned integer, representing the flags byte, or might use 'C<X>'
and 'C<Y>' for ECE and CWR, respectively.

Verson 1.0 of the IPSummaryDump file format expressed fragment offsets in
8-byte units, not bytes. Fields in old dumps were sometimes quoted and
contained spaces instead of underscores. In Version 1.2 files payload MD5
checksums were sometimes incorrect.

=head1 IP OPTIONS

Single IP option fields have the following representations.

    EOL, NOP        Not written, but FromIPSummaryDump
                    understands 'eol' and 'nop'

    RR              'rr{10.0.0.1,20.0.0.2}+5' (addresses
                    inside the braces come before the
		    pointer; '+5' means there is space for
		    5 more addresses after the pointer)

    SSRR, LSRR      'ssrr{1.0.0.1,1.0.0.2^1.0.0.3}'
                    ('^' indicates the pointer)

    TS              'ts{1,10000,!45}+2++3' (timestamps only
                    [type 0]; timestamp values 1, 10000,
		    and 45 [but 45 has the "nonstandard
		    timestamp" bit set]; the option has
		    room for 2 more timestamps; the
		    overflow counter is set to 3)

		    'ts.ip{1.0.0.1=1,1.0.0.2=2}+5'
		    (timestamps with IP addresses [type 1])

		    'ts.preip{1.0.0.1=1^1.0.0.2,1.0.0.3}'
		    (prespecified IP addresses [type 3];
		    the caret is the pointer)

    Other options   '98' (option 98, no data),
                    '99=0:5:10' (option with data, data
		    octets separated by colons)

Multiple options are separated by semicolons. (No single option will ever
contain a semicolon.) Any invalid option causes the entire field to be
replaced by a single question mark 'C<?>'. A period 'C<.>' is used for packets
with no options (except possibly EOL and NOP).

=head1 TCP OPTIONS

Single TCP option fields have the following representations.

    EOL, NOP        Not written, but FromIPSummaryDump
                    understands 'eol' and 'nop'
    MSS             'mss1400'
    Window scale    'wscale10'
    SACK permitted  'sackok'
    SACK            'sack95-98'; each SACK block
                    is listed separately
    Timestamp       'ts669063908:38382731'
    Other options   '98' (option 98, no data),
                    '99=0:5:10' (option with data, data
		    octets separated by colons)

Multiple options are separated by semicolons. (No single option will ever
contain a semicolon.) Any invalid option causes the entire field to be
replaced by a single question mark 'C<?>'. A period 'C<.>' is used for packets
with no options (except possibly EOL and NOP).

=head1 BINARY FORMAT

Binary IPSummaryDump files begin with several ASCII lines, just like regular
files. The line 'C<!binary>' indicates that the rest of the file, starting
immediately after the newline, consists of binary records. Each record looks
like this:

   +---------------+------------...
   |X|record length|    data
   +---------------+------------...
    <---4 bytes--->

The initial word of data contains the record length in bytes. (All numbers in
the file are stored in network byte order.) The record length includes the
initial word itself, so the minimum valid record length is 4. The high-order
bit 'C<X>' is the metadata indicator. It is zero for regular packets and one
for metadata lines.

Regular packet records have binary fields stored in the order indicated by
the 'C<!data>' line, as follows:

   Field Name    Length  Description
   timestamp	    8	 timestamp sec + usec
   utimestamp	    8	 timestamp sec + usec
   ntimestamp	    8	 timestamp sec + nsec
   ts_sec, ts_usec  4	 timestamp sec/usec
   ts_usec1         8    timestamp in usec
   ip_src           4    IP source address
   ip_dst           4    IP destination address
   sport            2    source port
   dport            2    destination port
   ip_len           4    IP length field
   ip_proto         1    IP protocol
   ip_id            2    IP ID
   ip_tos           1    IP TOS
   ip_ttl           1    IP TTL
   ip_frag          1    fragment descriptor
                         ('F', 'f', or '.')
   ip_fragoff       2    IP fragment offset field
   ip_opt           ?    IP options
   tcp_seq          4    TCP sequence number
   tcp_ack          4    TCP ack number
   tcp_flags        1    TCP flags
   tcp_opt          ?    TCP options
   tcp_ntopt        ?    TCP non-timestamp options
   tcp_sack         ?    TCP SACK options
   udp_len          4    UDP length
   payload_len      4    payload length
   payload_md5     16    payload MD5 checksum
   payload_md5_hex 16    payload MD5 checksum
   ip_capture_len   4    IP capture length
   count            4    packet count
   first_timestamp  8    timestamp sec + usec
   eth_src          6    Ethernet source address
   eth_dst          6    Ethernet destination address

Each field is Length bytes long. Variable-length fields have Length 'C<?>' in
the table; in a packet record, these fields consist of a single length byte,
followed by that many bytes of data.

The data stored in a metadata record is just an ASCII string, ending with
newline, same as in a regular ASCII IPSummaryDump file. 'C<!bad>' records, for
example, are stored this way.

=h flush write-only

Flush all internal buffers to disk.

=a

FromIPSummaryDump, FromDump, ToDump */

class ToIPSummaryDump : public Element, public IPSummaryDumpInfo { public:

    ToIPSummaryDump() CLICK_COLD;
    ~ToIPSummaryDump() CLICK_COLD;

    const char *class_name() const	{ return "ToIPSummaryDump"; }
    const char *port_count() const	{ return "1/0-1"; }
    const char *processing() const	{ return "a/h"; }
    const char *flags() const		{ return "S2"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int, Packet *);
    bool run_task(Task *);

    String filename() const		{ return _filename; }
    uint32_t output_count() const	{ return _output_count; }
    void add_note(const String &);
    void write_line(const String &);

  private:

    String _filename;
    FILE *_f;
    Vector<const IPSummaryDump::FieldWriter *> _fields;
    Vector<const IPSummaryDump::FieldWriter *> _prepare_fields;
    bool _verbose : 1;
    bool _bad_packets : 1;
    bool _careful_trunc : 1;
    bool _multipacket : 1;
    bool _active : 1;
    bool _binary : 1;
    bool _header : 1;
    bool _extra_length : 1;
    int32_t _binary_size;
    uint32_t _output_count;
    Task _task;
    NotifierSignal _signal;

    StringAccum _sa;
    StringAccum _bad_sa;

    String _banner;

    bool summary(Packet* p, StringAccum& sa, StringAccum* bad_sa) const;
    void write_packet(Packet* p, int multipacket);
    static int flush_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif

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

ToIPSummaryDump(FILENAME [, I<KEYWORDS>])

=s analysis

writes packet summary information

=d

Writes summary information about incoming packets to FILENAME in a simple
ASCII format---each line corresponds to a packet. The CONTENTS keyword
argument determines what information is written. Writes to standard output if
FILENAME is a single dash `C<->'.

ToIPSummaryDump uses packets' extra-length and extra-packet-count annotations.

Keyword arguments are:

=over 8

=item CONTENTS

Space-separated list of field names. Each line of the summary dump will
contain those fields. Valid field names, with examples, are:

   timestamp    Packet timestamp: `996033261.451094'
   ts_sec       Seconds portion of timestamp: `996033261'
   ts_usec      Microseconds portion of timestamp: `451094'
   ip_src       IP source address: `192.150.187.37'
   ip_dst       IP destination address: `192.168.1.100'
   ip_frag      IP fragment: `F' (1st frag), `f' (2nd or
                later frag), or `.' (not frag)
   ip_fragoff   IP fragmentation offset: `0', `0+' (suffix
                `+' means MF is set; offset in bytes)
   len          Packet length: `132'
   proto        IP protocol: `10', or `I' for ICMP, `T' for
                TCP, `U' for UDP
   ip_id        IP ID: `48759'
   sport        TCP/UDP source port: `22'
   dport        TCP/UDP destination port: `2943'
   tcp_seq      TCP sequence number: `93167339'
   tcp_ack      TCP acknowledgement number: `93178192'
   tcp_flags    TCP flags: `SA', `.'
   payload_len  Payload length (not including IP/TCP/UDP
                headers): `34'
   count        Number of packets: `1'
   direction    Link number (PAINT_ANNO): '2', or '>'/'L'
                for paint 0, '<'/'R'/'X' for paint 1
   aggregate    Aggregate number (AGGREGATE_ANNO): '973'

If a field does not apply to a particular packet -- for example, `C<sport>' on
an ICMP packet -- ToIPSummaryDump prints a single dash for that value.

Default CONTENTS is `src dst'. You may also use spaces instead of underscores,
in which case you must quote field names that contain a space -- for example,
`C<src dst "tcp seq">'.

=item VERBOSE

Boolean. If true, then print out a couple comments at the beginning of the
dump describing the hostname and starting time, in addition to the `C<!data>' line describing the log contents. Default is false.

=item BANNER

String. If supplied, prints a `C<!creator "BANNER">' comment at the beginning
of the dump.

=item BINARY

Boolean. If true, then output packet records in a binary format (explained
below). Defaults to false.

=item MULTIPACKET

Boolean. If true, and the CONTENTS option doesn't contain `C<count>', then
generate multiple summary entries for packets with nonzero packet count
annotations. For example, if MULTIPACKET is true, and a packet has packet
count annotation 2, then ToIPSummaryDump will generate 2 identical lines for
that packet in the dump. False by default.

=item BAD_PACKETS

Boolean. If true, then print `C<!bad MESSAGE>' lines for packets with bad IP,
TCP, or UDP headers, instead of normal output. (Even if BAD_PACKETS is false,
output will contain dashes `C<->' in place of data from bad headers.) Default
is false.

=item CAREFUL_TRUNC

Boolean. If true, then print `C<!bad truncated IP length>' lines for packets
whose data plus extra length annotation is less than their IP length.
B<Tcpdump> prints `C<truncated-ip - N bytes missing>' for such packets.
Default is true.

=back

=e

Here are a couple lines from the start of a sample verbose dump.

  !IPSummaryDump 1.1
  !creator "aciri-ipsumdump -i wvlan0"
  !host no.lcdf.org
  !runtime 996022410.322317 (Tue Jul 24 17:53:30 2001)
  !data 'ip src' 'ip dst'
  63.250.213.167 192.150.187.106
  63.250.213.167 192.150.187.106

The end of the dump may contain a comment `C<!drops N>', meaning that C<N>
packets were dropped before they could be entered into the dump.

A `C<!flowid>' comment can specify source and destination addresses and ports
for packets that otherwise don't have one.

=n

The `C<len>' and `C<payload_len>' content types use the extra length
annotation. The `C<count>' content type uses the packet count annotation.

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
   CWR        W          0x80

Some old IP summary dumps might contain an unsigned integer, representing the
flags byte, or might use characters X and Y for flags ECE and CWR,
respectively.

Verson 1.0 of the IPSummaryDump file format expressed fragment offsets in
8-byte units, not bytes. Content types in old dumps were sometimes quoted and
contained spaces instead of underscores.

=head1 BINARY FORMAT

Binary IPSummaryDump files begin with several ASCII lines, just like regular
files. The line `C<!binary>' indicates that the rest of the file, starting
immediately after the newline, consists of binary records. (`C<!binary>' may
be followed by several space characters to ensure that the first binary record
begins on a 4-byte boundary.) Each record is a multiple of 4 bytes long, and
looks like this:

   +---------------+------------...
   |X|record length|    data
   +---------------+------------...
    <---4 bytes--->

The initial word of data stores the record length in words. (All numbers in
the file are stored in network byte order.) The record length includes the
initial word itself, so the minimum valid record length is 1. The
high-order bit `C<X>' is the metadata indicator. It is zero for regular
packets and one for metadata lines.

Regular packet records have binary fields stored in the order indicated by
the `C<!data>' line, as follows:

   Field Name  Length Align  Description
   timestamp      8     4    timestamp sec, usec
   ip_src         4     4    source IP address
   ip_dst         4     4    destination IP address
   sport          2     2    source port
   dport          2     2    destination port
   ip_len         4     4    IP length field
   ip_proto       1     1    IP protocol
   ip_id          2     2    IP ID
   ip_frag        1     1    fragment descriptor
                             ('F', 'f', or '.')
   ip_fragoff     2     2    IP fragment offset field
   tcp_seq        4     4    TCP seqnece number
   tcp_ack        4     4    TCP ack number
   tcp_flags      1     1    TCP flags
   payload_len    4     4    payload length
   count          4     4    packet count

Each field is Length bytes long, and aligned on an Align-byte boundary,
possibly by introducing padding between fields. Some CONTENTS orders may
introduce unnecessary padding. For example, the records for CONTENTS
`C<sport src dport>' will be 12 bytes long (because `C<sport>' is
padded by two bytes so `C<src>' can start on a 4-byte boundary), but the
records for `C<src sport dport>' will be 8 bytes long.

The data stored in a metadata record is just an ASCII string, ending with
newline (possibly padded with zero bytes on the right), same as in a regular
ASCII IPSummaryDump file. For instance, `C<!bad>' records are stored this
way.

=a

FromDump, ToDump */

class ToIPSummaryDump : public Element, public IPSummaryDumpInfo { public:

    ToIPSummaryDump();
    ~ToIPSummaryDump();

    const char *class_name() const	{ return "ToIPSummaryDump"; }
    const char *processing() const	{ return AGNOSTIC; }
    const char *flags() const		{ return "S2"; }
    ToIPSummaryDump *clone() const	{ return new ToIPSummaryDump; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void push(int, Packet *);
    void run_scheduled();

    uint32_t output_count() const	{ return _output_count; }
    void write_line(const String &);
    void flush_buffer();

  private:

    String _filename;
    FILE *_f;
    StringAccum _sa;
    Vector<unsigned> _contents;
    bool _verbose : 1;
    bool _bad_packets : 1;
    bool _careful_trunc : 1;
    bool _multipacket : 1;
    bool _active : 1;
    bool _binary : 1;
    int32_t _binary_size;
    uint32_t _output_count;
    Task _task;
    NotifierSignal _signal;
    
    String _banner;

    bool ascii_summary(Packet *, StringAccum &) const;
    bool binary_summary(Packet *, const click_ip *, const click_tcp *, const click_udp *, StringAccum &) const;
    bool bad_packet(StringAccum &, const String &, int) const;
    void write_packet(Packet *, bool multipacket = false);
    
};

CLICK_ENDDECLS
#endif

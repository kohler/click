// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TOIPSUMDUMP_HH
#define CLICK_TOIPSUMDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/straccum.hh>
#include <click/notifier.hh>
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

The `C<len>' and `C<payload len>' content types use the extra length
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
8-byte units, not bytes.

=a

FromDump, ToDump */

class ToIPSummaryDump : public Element { public:

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
    void write_string(const String &);
    void flush_buffer();

    enum Content {		// must agree with FromIPSummaryDump
	W_NONE, W_TIMESTAMP, W_TIMESTAMP_SEC, W_TIMESTAMP_USEC,
	W_SRC, W_DST, W_LENGTH, W_PROTO, W_IPID,
	W_SPORT, W_DPORT, W_TCP_SEQ, W_TCP_ACK, W_TCP_FLAGS,
	W_PAYLOAD_LENGTH, W_COUNT, W_FRAG, W_FRAGOFF,
	W_PAYLOAD, W_LINK, W_AGGREGATE,
	W_LAST
    };

  private:

    String _filename;
    FILE *_f;
    StringAccum _sa;
    Vector<unsigned> _contents;
    bool _multipacket;
    bool _active;
    uint32_t _output_count;
    Task _task;
    NotifierSignal _signal;
    bool _verbose : 1;
    bool _bad_packets : 1;
    bool _careful_trunc : 1;
    
    String _banner;

    bool ascii_summary(Packet *, StringAccum &) const;
    bool bad_packet(StringAccum &, const String &, int) const;
    void write_packet(Packet *, bool multipacket = false);
    
};

CLICK_ENDDECLS
#endif

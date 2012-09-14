#ifndef CLICK_IPPRINT_HH
#define CLICK_IPPRINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

IPPrint([LABEL, I<KEYWORDS>])

=s ip

pretty-prints IP packets

=d

Expects IP packets as input.  Should be placed downstream of a
CheckIPHeader or equivalent element.

Prints out IP packets in a human-readable tcpdump-like format, preceded by
the LABEL text.

Keyword arguments are:

=over 8

=item CONTENTS

Determines whether the packet data is printed. It may be `NONE' (do not print
packet data), `HEX' (print packet data in hexadecimal), or `ASCII' (print
packet data in plaintext). Default is `NONE'.

=item PAYLOAD

Like CONTENTS, but prints only the packet payload, rather than the entire
packet. Specify at most one of CONTENTS and PAYLOAD.

=item MAXLENGTH

If CONTENTS or PAYLOAD printing is on, then MAXLENGTH determines the maximum
number of bytes to print. -1 means print the entire packet or payload. Default
is 1500.

=item ID

Boolean. Determines whether to print each packet's IP ID field. Default is
false.

=item TTL

Boolean. Determines whether to print each packet's IP TOS field. Default is
false.

=item TOS

Boolean. Determines whether to print each packet's IP TOS field. Default is
false.

=item LENGTH

Boolean. Determines whether to print each packet's IP length field. Default is
false.

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is true.

=item AGGREGATE

Boolean. Determines whether to print each packet's aggregate annotation.
Default is false.

=item PAINT

Boolean. Determines whether to print each packet's paint annotation. Default is false.

=item SWAP

Boolean.  Determines whether to swap bytes before printing the values
of ICMP sequence and ID numbers.  Default is false.  The RFC does not
require these two-byte values to be sent in any particular byte order.
For example, OpenBSD/i386 writes ping (ICMP echo) sequence numbers in
network byte order, while Linux/i386 writes them in host byte order
(often little-endian).

=item OUTFILE

String. Only available at user level. PrintV<> information to the file specified
by OUTFILE instead of standard error.

=item ACTIVE

Boolean.  If false, then don't print messages.  Default is true.

=back

=a Print, CheckIPHeader */

class IPPrint : public Element { public:

  IPPrint() CLICK_COLD;
  ~IPPrint() CLICK_COLD;

  const char *class_name() const		{ return "IPPrint"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  bool _swap;
  bool _active;
  String _label;
  int _bytes;			// Number of bytes to dump
  bool _print_id : 1;		// Print IP ID?
  bool _print_timestamp : 1;
  bool _print_paint : 1;
  bool _print_tos : 1;
  bool _print_ttl : 1;
  bool _print_len : 1;
  bool _print_aggregate : 1;
  bool _payload : 1;		// '_contents' refers to payload
  unsigned _contents : 2;	// Whether to dump packet contents

#if CLICK_USERLEVEL
  String _outfilename;
  FILE *_outfile;
#endif
  ErrorHandler *_errh;

    static StringAccum &address_pair(StringAccum &sa, const click_ip *iph);
    void tcp_line(StringAccum &, const Packet *, int transport_len) const;
    void udp_line(StringAccum &, const Packet *, int transport_len) const;
    void icmp_line(StringAccum &, const Packet *, int transport_len) const;

};

CLICK_ENDDECLS
#endif

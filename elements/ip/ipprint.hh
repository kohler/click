#ifndef IPPRINT_HH
#define IPPRINT_HH
#include <click/element.hh>
#include <click/string.hh>

/*
=c

IPPrint([TAG, I<KEYWORDS>])

=s IP, debugging

pretty-prints IP packets

=d

Expects IP packets as input.  Should be placed downstream of a 
CheckIPHeader or equivalent element.

Prints out IP packets in a human-readable tcpdump-like format, preceded by
the TAG text.

Keyword arguments are:

=over 8

=item CONTENTS

Determines whether the packet data is printed. It may be `false' (do not print
packet data), `hex' (print packet data in hexadecimal), or `ascii' (print
packet data in plaintext). Default is `false'.

=item NBYTES

If CONTENTS is `hex' or `ascii', then NBYTES determines the number of bytes to
dump. Default is 1500.

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
1970. Default is false.

=item SWAP

Boolean.  Determines whether to swap bytes before printing the values
of ICMP sequence and ID numbers.  Default is false.  The RFC does not
require these two-byte values to be sent in any particular byte order.
For example, OpenBSD/i386 writes ping (ICMP echo) sequence numbers in
network byte order, while Linux/i386 writes them in host byte order
(often little-endian).

=item OUTFILE

String. Only available at user level. Print information to the file specified
by OUTFILE instead of standard error.

=back

=a Print, CheckIPHeader */

class IPPrint : public Element { public:
  
  IPPrint();
  ~IPPrint();
  
  const char *class_name() const		{ return "IPPrint"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPPrint *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  Packet *simple_action(Packet *);

 private:

  bool _swap;
  String _label;
  unsigned _bytes;		// Number of bytes to dump
  bool _print_id : 1;		// Print IP ID?
  bool _print_timestamp : 1;
  bool _print_paint : 1;
  bool _print_tos : 1;
  bool _print_ttl : 1;
  bool _print_len : 1;
  unsigned _contents : 2;	// Whether to dump packet contents

#if CLICK_USERLEVEL
  String _outfilename;
  FILE *_outfile;
#endif
  ErrorHandler *_errh;

};

#endif

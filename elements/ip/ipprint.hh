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

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is false.

=back

=a Print, CheckIPHeader */

class IPPrint : public Element { public:
  
  IPPrint();
  ~IPPrint();
  
  const char *class_name() const		{ return "IPPrint"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPPrint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  Packet *simple_action(Packet *);

 private:

  String _label;
  char *_buf;			// To hold hex dump message
  unsigned _bytes;		// Number of bytes to dump
  bool _print_id : 1;		// Print IP ID?
  bool _print_timestamp : 1;
  bool _print_paint : 1;
  unsigned _contents : 2;	// Whether to dump packet contents

};

#endif

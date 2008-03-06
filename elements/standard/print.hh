#ifndef CLICK_PRINT_HH
#define CLICK_PRINT_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
=c

Print([LABEL, MAXLENGTH, I<keywords>])

=s debugging

prints packet contents

=d

Prints up to LENGTH bytes of data from each packet, in hex, preceded by the
LABEL text. Default LENGTH is 24.

Keyword arguments are:

=over 8

=item MAXLENGTH

Maximum number of content bytes to print. If negative, print entire
packet. Default is 24.

=item CONTENTS

Determines whether the packet data is printed. It may be `NONE' (do not print
packet data), `HEX' (print packet data in hexadecimal), or `ASCII' (print
packet data in plaintext). Default is `HEX'.

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is false.

=item PRINTANNO

Boolean. Determines whether to print each packet's user annotation bytes.  Default is false.

=item CPU

Boolean; available only in the Linux kernel module. Determines whether to
print the current CPU ID for every packet. Default is false.

=back

=a

IPPrint */

class Print : public Element { public:

  Print();
  ~Print();
  
  const char *class_name() const		{ return "Print"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *simple_action(Packet *);
  
 private:
  
    String _label;
    int _bytes;		// How many bytes of a packet to print
    bool _timestamp : 1;
#ifdef CLICK_LINUXMODULE
    bool _cpu : 1;
#endif
    bool _print_anno;
    uint8_t _contents;
};

CLICK_ENDDECLS
#endif

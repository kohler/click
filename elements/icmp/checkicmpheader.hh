#ifndef CLICK_CHECKICMPHEADER_HH
#define CLICK_CHECKICMPHEADER_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
=c
CheckICMPHeader([I<KEYWORDS>])

=s icmp

checks ICMP header on ICMP packets

=d

Expects ICMP packets as input. Checks that the packet's length is sensible
and that its checksum field is valid. Pushes invalid packets out on output
1, unless output 1 was unused; if so, drops invalid packets.

Prints a message to the console the first time it encounters an incorrect IP
packet (but see VERBOSE below).

Keyword arguments are:

=over 5

=item VERBOSE

Boolean. If it is true, then a message will be printed for every erroneous
packet, rather than just the first. False by default.

=item DETAILS

Boolean. If it is true, then CheckICMPHeader will maintain detailed counts of
how many packets were dropped for each possible reason, accessible through the
C<drop_details> handler. False by default.

=back

=h drops read-only

Returns the number of incorrect packets CheckICMPHeader has seen.

=h drop_details read-only

Returns a text file showing how many erroneous packets CheckICMPHeader has seen,
subdivided by error. Only available if the DETAILS keyword argument was true.

=a CheckIPHeader, CheckTCPHeader, CheckUDPHeader, MarkIPHeader */

class CheckICMPHeader : public Element { public:

  CheckICMPHeader() CLICK_COLD;
  ~CheckICMPHeader() CLICK_COLD;

  const char *class_name() const		{ return "CheckICMPHeader"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  bool _verbose : 1;
  atomic_uint32_t _drops;
  atomic_uint32_t *_reason_drops;

  enum Reason {
    NOT_ICMP,
    BAD_LENGTH,
    BAD_CHECKSUM,
    NREASONS
  };
  static const char *reason_texts[NREASONS];

  Packet *drop(Reason, Packet *);
  static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

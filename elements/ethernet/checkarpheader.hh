#ifndef CLICK_CHECKARPHEADER_HH
#define CLICK_CHECKARPHEADER_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
=c

CheckARPHeader([OFFSET, I<keywords> OFFSET, VERBOSE, DETAILS])

=s arp

checks ARP header

=d

Input packets should have ARP headers starting OFFSET bytes in. Default OFFSET
is zero. Checks that the packet's length is reasonable.

CheckARPHeader emits valid packets on output 0. Invalid packets are pushed out
on output 1, unless output 1 was unused; if so, drops invalid packets.

CheckARPHeader prints a message to the console the first time it encounters an
incorrect ARP packet (but see VERBOSE below).

Keyword arguments are:

=over 5

=item OFFSET

Unsigned integer. Byte position at which the ARP header begins. Default is 0.

=item VERBOSE

Boolean. If it is true, then a message will be printed for every erroneous
packet, rather than just the first. False by default.

=item DETAILS

Boolean. If it is true, then CheckARPHeader will maintain detailed counts of
how many packets were dropped for each possible reason, accessible through the
C<drop_details> handler. False by default.

=back

=n

=h drops read-only

Returns the number of incorrect packets CheckARPHeader has seen.

=h drop_details read-only

Returns a text file showing how many erroneous packets CheckARPHeader has seen,
subdivided by error. Only available if the DETAILS keyword argument was true.

=a

ARPPrint, ARPQuerier, ARPResponder, ARPFaker

*/

class CheckARPHeader : public Element { public:

  CheckARPHeader() CLICK_COLD;
  ~CheckARPHeader() CLICK_COLD;

    const char *class_name() const		{ return "CheckARPHeader"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  unsigned _offset;

  bool _verbose : 1;

  atomic_uint32_t _drops;
  atomic_uint32_t *_reason_drops;

  enum Reason {
    MINISCULE_PACKET,
    BAD_LENGTH,
    BAD_HRD,
    BAD_PRO,
    NREASONS
  };
  static const char * const reason_texts[NREASONS];

  Packet *drop(Reason, Packet *);
  static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

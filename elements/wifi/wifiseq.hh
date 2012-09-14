#ifndef CLICK_WIFISEQ_HH
#define CLICK_WIFISEQ_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

WifiSeq()

=s Wifi

Sets the 802.11 sequence number in a packet.

=d

Arguments are:

=over 8
=item OFFSET
How many bytes in the packet to put the seuqnce number. Default is 22.

=item BYTES
How many bytes the sequence number should use. Values can be
2 or 4. Default is 2.

=item SHIFT
How many bits to shift the sequence number before setting the value in the packet.
Default is 4.

=back 8


=h seq read/write
Sets or reads the next sequence number

=a WifiEncap */

class WifiSeq : public Element { public:

  WifiSeq() CLICK_COLD;
  ~WifiSeq() CLICK_COLD;

  const char *class_name() const	{ return "WifiSeq"; }
  const char *port_count() const	{ return "1/-"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;
  static String read_param(Element *, void *) CLICK_COLD;
  static int write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh);

  Packet *apply_seq(Packet *);

  void reset();
  bool _debug;

  u_int32_t _seq;
  u_int32_t _offset;
  u_int32_t _shift;
  u_int32_t _bytes;

};

CLICK_ENDDECLS
#endif

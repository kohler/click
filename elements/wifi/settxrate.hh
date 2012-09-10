#ifndef CLICK_SETTXRATE_HH
#define CLICK_SETTXRATE_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
SetTXRate([I<KEYWORDS>])

=s Wifi

Sets the bit-rate for a packet.

=d

Sets the Wifi TXRate Annotation on a packet.

Regular Arguments:
=over 8

=item RATE
Unsigned integer. Rate value is multiplied by 2 (i.e. 2
means 1 Mbps, 11 means 5.5 Mbps)

=back 8

=h rate read/write
Same as RATE Argument

=a AutoRateFallback, MadwifiRate, ProbeRate, ExtraEncap
*/

class SetTXRate : public Element { public:

  SetTXRate() CLICK_COLD;
  ~SetTXRate() CLICK_COLD;

  const char *class_name() const		{ return "SetTXRate"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;
  static String read_handler(Element *e, void *) CLICK_COLD;
  static int write_handler(const String &arg, Element *e,
			   void *, ErrorHandler *errh);
private:

  int _rate;
  int _tries;
  uint16_t _et;     // This protocol's ethertype
  unsigned _offset;
};

CLICK_ENDDECLS
#endif

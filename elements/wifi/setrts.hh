#ifndef CLICK_SETRTS_HH
#define CLICK_SETRTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

/*
=c

SetRTS(Bool)

=s Wifi

Enable/disable RTS/CTS for a packet

=d

Enable/disable RTS/CTS for a packet

=h rts read/write
Enable/disable rts/cts for a packet.

=a ExtraEncap, ExtraDecap
*/

class SetRTS : public Element { public:

  SetRTS() CLICK_COLD;
  ~SetRTS() CLICK_COLD;

  const char *class_name() const		{ return "SetRTS"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;
  bool _rts;
};

CLICK_ENDDECLS
#endif

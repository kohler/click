#ifndef CLICK_PUSHANNO_HH
#define CLICK_PUSHANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
PushAnno([I<KEYWORDS>])
=s Wifi
Pushes Packet::anno() onto front of packet.

=d
Pushes Packet::USER_ANNO_SIZE bytes on a packet and copies
Packet::anno() to the beginning of the packet.

=e
FromDevice(ath0) -> PushAnno() -> ToDump()

=a ReadAnno
*/


class PushAnno : public Element { public:

  PushAnno() CLICK_COLD;
  ~PushAnno() CLICK_COLD;

  const char *class_name() const		{ return "PushAnno"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return "a/a"; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif

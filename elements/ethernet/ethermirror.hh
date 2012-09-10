#ifndef CLICK_ETHERMIRROR_HH
#define CLICK_ETHERMIRROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * EtherMirror()
 * =s ethernet
 * swaps Ethernet source and destination
 * =d
 *
 * Incoming packets are Ethernet. Their source and destination Ethernet
 * addresses are swapped before they are output.
 * */

class EtherMirror : public Element { public:

  EtherMirror() CLICK_COLD;
  ~EtherMirror() CLICK_COLD;

  const char *class_name() const	{ return "EtherMirror"; }
  const char *port_count() const	{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif // CLICK_ETHERMIRROR_HH

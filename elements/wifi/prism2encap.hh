#ifndef CLICK_PRISM2ENCAP_HH
#define CLICK_PRISM2ENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

Prism2Encap

=s Wifi

Pushes a Prism2 header onto a packet based on information stored in Packet::anno()

=d
Removes the prism2 header and sets the corresponding wifi packet annotations (RSSI, NOISE, and RATE).

=a Prism2Decap, ExtraDecap, ExtraEncap
 */

class Prism2Encap : public Element { public:

  Prism2Encap();
  ~Prism2Encap();

  const char *class_name() const	{ return "Prism2Encap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif

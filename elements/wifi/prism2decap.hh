#ifndef CLICK_PRISM2DECAP_HH
#define CLICK_PRISM2DECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

Prism2Decap

=s Wifi

Pulls the prism2 header from a packet and store information in Packet::anno()

=d
Removes the prism2 header and sets the corresponding wifi packet annotations (RSSI, NOISE, and RATE).


=a Prism2Encap, ExtraDecap, ExtraEncap
*/

class Prism2Decap : public Element { public:

  Prism2Decap();
  ~Prism2Decap();

  const char *class_name() const	{ return "Prism2Decap"; }
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

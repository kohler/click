#ifndef CLICK_PRISM2ENCAP_HH
#define CLICK_PRISM2ENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

Prism2Encap()

=s encapsulation, Prism2 -> 802.11

Removes the prism2 header and sets the corresponding wifi packet annotations (RSSI, NOISE, and RATE).
=d

=a

EtherEncap */

class Prism2Encap : public Element { public:
  
  Prism2Encap();
  ~Prism2Encap();

  const char *class_name() const	{ return "Prism2Encap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif

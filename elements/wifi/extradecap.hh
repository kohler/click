#ifndef CLICK_EXTRADECAP_HH
#define CLICK_EXTRADECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

ExtraDecap()

=s decapsulation, Extra -> 802.11

Removes the extra header and sets the corresponding wifi packet annotations (RSSI, NOISE, and RATE).
=d

=a

EtherEncap */

class ExtraDecap : public Element { public:
  
  ExtraDecap();
  ~ExtraDecap();

  const char *class_name() const	{ return "ExtraDecap"; }
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

#ifndef CLICK_EXTRAENCAP_HH
#define CLICK_EXTRAENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

ExtraEncap()

=s encapsulation, Extra -> 802.11

Removes the extra header and sets the corresponding wifi packet annotations (RSSI, NOISE, and RATE).
=d

=a

EtherEncap */

class ExtraEncap : public Element { public:
  
  ExtraEncap();
  ~ExtraEncap();

  const char *class_name() const	{ return "ExtraEncap"; }
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

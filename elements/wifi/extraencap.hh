#ifndef CLICK_EXTRAENCAP_HH
#define CLICK_EXTRAENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
ExtraEncap()

=s Wifi

Pushes the prism2 header on a packet based on information in Packet::anno()

=d

Copies the wifi_extra_header from Packet::anno() and pushes it onto the packet.

=a ExtraDecap, SetTXRate
*/

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

#ifndef CLICK_EXTRAENCAP_HH
#define CLICK_EXTRAENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
ExtraEncap()

=s Wifi

Pushes the click_wifi_extra header on a packet based on information in Packet::anno()

=d

Copies the wifi_extra_header from Packet::anno() and pushes it onto the packet.

=a ExtraDecap, SetTXRate
*/

class ExtraEncap : public Element { public:

  ExtraEncap() CLICK_COLD;
  ~ExtraEncap() CLICK_COLD;

  const char *class_name() const	{ return "ExtraEncap"; }
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

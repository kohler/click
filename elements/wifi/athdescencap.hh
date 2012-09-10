#ifndef CLICK_ATHDESCENCAP_HH
#define CLICK_ATHDESCENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
AthdescEncap()

=s Wifi

Pushes the click_wifi_radiotap header on a packet based on information in Packet::anno()

=d

Copies the wifi_radiotap_header from Packet::anno() and pushes it onto the packet.

=a AthdescDecap, SetTXRate
*/


class AthdescEncap : public Element { public:

  AthdescEncap() CLICK_COLD;
  ~AthdescEncap() CLICK_COLD;

  const char *class_name() const	{ return "AthdescEncap"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif

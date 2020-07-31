#ifndef CLICK_RADIOTAPENCAP_HH
#define CLICK_RADIOTAPENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
RadiotapEncap()

=s Wifi

Pushes the click_wifi_radiotap header on a packet based on information in Packet::anno()

=d

Copies the wifi_radiotap_header from Packet::anno() and pushes it onto the packet.

=a RadiotapDecap, SetTXRate
*/


class RadiotapEncap : public Element { public:

  RadiotapEncap() CLICK_COLD;
  ~RadiotapEncap() CLICK_COLD;

  const char *class_name() const	{ return "RadiotapEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);
  Packet *encap(Packet *);
  Packet *encap_ht(Packet *);

  bool _debug;

};

CLICK_ENDDECLS
#endif

#ifndef CLICK_ATHDESCDECAP_HH
#define CLICK_ATHDESCDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
AthdescDecap()

=s Wifi

Pulls the click_wifi_athdesc header from a packet and stores it in Packet::anno()

=d
Removes the athdesc header and copies to to Packet->anno(). This contains
informatino such as rssi, noise, bitrate, etc.

=a AthdescEncap
*/

class AthdescDecap : public Element { public:

  AthdescDecap() CLICK_COLD;
  ~AthdescDecap() CLICK_COLD;

  const char *class_name() const	{ return "AthdescDecap"; }
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

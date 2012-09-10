#ifndef CLICK_EXTRADECAP_HH
#define CLICK_EXTRADECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
ExtraDecap()

=s Wifi

Pulls the click_wifi_extra header from a packet and stores it in Packet::anno()

=d
Removes the extra header and copies to to Packet->anno(). This contains
informatino such as rssi, noise, bitrate, etc.

=a ExtraEncap
*/

class ExtraDecap : public Element { public:

  ExtraDecap() CLICK_COLD;
  ~ExtraDecap() CLICK_COLD;

  const char *class_name() const	{ return "ExtraDecap"; }
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

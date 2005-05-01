#ifndef CLICK_RADIOTAPDECAP_HH
#define CLICK_RADIOTAPDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
RadiotapDecap()

=s Wifi

Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()

=d
Removes the radiotap header and copies to to Packet->anno(). This contains
informatino such as rssi, noise, bitrate, etc.

=a RadiotapEncap
*/

class RadiotapDecap : public Element { public:
  
  RadiotapDecap();
  ~RadiotapDecap();

  const char *class_name() const	{ return "RadiotapDecap"; }
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

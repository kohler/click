#ifndef CLICK_WEPENCAP_HH
#define CLICK_WEPENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <elements/wifi/rc4.hh>
CLICK_DECLS

/*
=c

WepEncap

=s Wifi

Turns 802.11 packets into ethernet packets

=d

=e


  wifi_cl :: Classifier (0/00%0c,
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_decap :: WepEncap() -> ...

=a WifiEncap
 */

class WepEncap : public Element { public:

  WepEncap() CLICK_COLD;
  ~WepEncap() CLICK_COLD;

  const char *class_name() const	{ return "WepEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  bool _debug;
  bool _strict;


  u_int32_t iv;
  struct rc4_state _rc4;
  String _key;
  int _keyid;
  bool _active;
 private:

};

CLICK_ENDDECLS
#endif

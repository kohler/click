#ifndef CLICK_WIFIENCAP_HH
#define CLICK_WIFIENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

WifiEncap(BSSID)

=s encapsulation, Wifi -> Ethernet

Turns 80211 packets into ethernet packets encapsulates packets in Ethernet header

=d

=e


  wifi_cl :: Classifier (0/00%0c, 
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt 
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_encap :: WifiEncap() -> ...

=a

EtherEncap */

class WifiEncap : public Element { public:
  
  WifiEncap();
  ~WifiEncap();

  const char *class_name() const	{ return "WifiEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;
  EtherAddress _bssid;
 private:

};

CLICK_ENDDECLS
#endif

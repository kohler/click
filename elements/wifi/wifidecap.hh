#ifndef CLICK_WIFIDECAP_HH
#define CLICK_WIFIDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

WifiDecap(ETHERTYPE, SRC, DST)

=s decapsulation, Wifi -> Ethernet

Turns 80211 packets into ethernet packets encapsulates packets in Ethernet header

=d

=e


  wifi_cl :: Classifier (0/00%0c, 
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt 
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_decap :: WifiDecap() -> ...

=a

EtherEncap */

class WifiDecap : public Element { public:
  
  WifiDecap();
  ~WifiDecap();

  const char *class_name() const	{ return "WifiDecap"; }
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

#ifndef CLICK_WIFIENCAP_HH
#define CLICK_WIFIENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

WifiEncap(MODE, BSSID)

=s Wifi, Encapsulation

Converts ethernet packets to 802.11 packets.

=d

Keyword arguments are:

=over 8
=item MODE
Mode is one of:
0x00 STA->STA
0x01 STA->AP
0x02 AP->STA
0x03 AP->AP

=item BSSID 

is an ethernet address

=back 8


=e
  wifi_cl :: Classifier (0/00%0c, 
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt 
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_encap :: WifiEncap() -> ...

=a EtherEncap */

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

  unsigned _mode;
  EtherAddress _bssid;
 private:

};

CLICK_ENDDECLS
#endif

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

Converts ethernet packets to 802.11 packets, with a LLC header.

=d

Arguments are:

=over 8
=item BSSID 
is an ethernet address. This ususally the access point's ethernet address.
If you are using Mode 0, this is usually set to 00:00:00:00:00:00 for
"psuedo-ibss" mode.

=item MODE
This specifies which address field the BSSID field is located at (for
instance, the BSSID is the destination when MODE is 1, because the
packet is going to the access point).
It should be one of:
N<>0 station -> station
N<>1 station -> access point
N<>2 access point -> station
N<>3 access point -> access point

=back 8


=h mode read/write
Same as MODE argument

=h bssid read/write
Same as BSSID argument

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
  class WirelessInfo *_winfo;
 private:

};

CLICK_ENDDECLS
#endif

#ifndef CLICK_BEACONSOURCE_HH
#define CLICK_BEACONSOURCE_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

BeaconSource(CHANNEL)

=s decapsulation, Wifi -> Ethernet

Turns 80211 packets into ethernet packets encapsulates packets in Ethernet header

=d


If channel is 0, it doesn't filter any beacons.
If channel is < 0, it doesn't look at any beconds
if channel is > 0, it looks at only beacons with on channel.
=e


  wifi_cl :: Classifier (0/00%0c, 
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt 
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_decap :: BeaconSource() -> ...

=a

EtherEncap */

class BeaconSource : public Element { public:
  
  BeaconSource();
  ~BeaconSource();

  const char *class_name() const	{ return "BeaconSource"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers();
  void run_timer();
  int initialize (ErrorHandler *);

  void send_beacon();


  Timer _timer;
  bool _debug;
  int _channel;
  EtherAddress _bssid;
  String _ssid;
  class AvailableRates *_rtable;
  int _interval_ms;


  String scan_string();
 private:


};

CLICK_ENDDECLS
#endif

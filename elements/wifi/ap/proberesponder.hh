#ifndef CLICK_PROBERESPONDER_HH
#define CLICK_PROBERESPONDER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

ProbeResponder(CHANNEL)

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
  wifi_cl [2] -> wifi_decap :: ProbeResponder() -> ...

=a

EtherEncap */

class ProbeResponder : public Element { public:
  
  ProbeResponder();
  ~ProbeResponder();

  const char *class_name() const	{ return "ProbeResponder"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers();

  void send_probe_response(EtherAddress);
  void push(int, Packet *);


  bool _debug;
  int _channel;
  EtherAddress _bssid;
  String _ssid;
  Vector<int> _rates;
  int _interval_ms;


  String scan_string();
 private:


};

CLICK_ENDDECLS
#endif

#ifndef CLICK_BEACONSCANNER_HH
#define CLICK_BEACONSCANNER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

BeaconScanner

=s Wifi, Wireless Station

Listens for 802.11 beacons and sends probe requests.

=d

=h scan read only
Statistics about access points that the element has received beacons from.

=h reset read/write
Clear the list of access points.

=h channel
If the channel is greater than 0, it will only record statistics for 
beacons received with that channel in the packet.
If channel is 0, it will record statistics for all beacons received.
If channel is less than 0, it will discard all beaconds

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
  wifi_cl [2] -> wifi_decap :: BeaconScanner() -> ...

=a

EtherEncap */

class BeaconScanner : public Element { public:
  
  BeaconScanner();
  ~BeaconScanner();

  const char *class_name() const	{ return "BeaconScanner"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();
  void reset();

  bool _debug;
  int _channel;

  String scan_string();
 private:



  class wap {
  public:
    EtherAddress _eth;
    String _ssid;
    int _channel;
    uint16_t _capability;
    uint16_t _beacon_int;
    Vector<int> _rates;
    Vector<int> _basic_rates;
    int _rssi;
    struct timeval _last_rx;
  };

  typedef HashMap<EtherAddress, wap> APTable;
  typedef APTable::const_iterator APIter;
  
  class APTable _waps;
  class AvailableRates *_rtable;
};

CLICK_ENDDECLS
#endif

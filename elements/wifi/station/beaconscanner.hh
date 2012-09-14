#ifndef CLICK_BEACONSCANNER_HH
#define CLICK_BEACONSCANNER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
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


=e

  FromDevice(ath0)
  -> Prism2Decap()
  -> ExtraDecap()
  -> Classifier(0/80%f0)  // only beacon packets
  -> bs :: BeaconScanner()
  -> Discard;

=a

EtherEncap */

class BeaconScanner : public Element { public:

  BeaconScanner() CLICK_COLD;
  ~BeaconScanner() CLICK_COLD;

  const char *class_name() const	{ return "BeaconScanner"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;
  void reset();

  bool _debug;

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
    Timestamp _last_rx;
  };

  typedef HashMap<EtherAddress, wap> APTable;
  typedef APTable::const_iterator APIter;

  APTable _waps;
  AvailableRates *_rtable;
  WirelessInfo *_winfo;
};

CLICK_ENDDECLS
#endif

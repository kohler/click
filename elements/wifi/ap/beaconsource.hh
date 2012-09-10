#ifndef CLICK_BEACONSOURCE_HH
#define CLICK_BEACONSOURCE_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

BeaconSource([, I<KEYWORDS>])

=s Wifi, Wireless AccessPoint

Send 802.11 beacons.

=d

Keyword arguments are:

=over 8

=item CHANNEL
The wireless channel it is operating on.

=item SSID
The SSID of the access point.

=item BSSID
An Ethernet Address (usually the same as the ethernet address of the wireless card).

=item INTERVAL
How often beacon packets are sent, in milliseconds.

=back 8

=a BeaconScanner

*/

class BeaconSource : public Element { public:

  BeaconSource() CLICK_COLD;
  ~BeaconSource() CLICK_COLD;

  const char *class_name() const	{ return "BeaconSource"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers() CLICK_COLD;
  void run_timer(Timer *);
  int initialize (ErrorHandler *);

  void send_beacon(EtherAddress, bool);
  void push(int, Packet *);


  Timer _timer;
  bool _debug;


  EtherAddress _bcast;
  String scan_string();

  class WirelessInfo *_winfo;
  class AvailableRates *_rtable;

 private:


};

CLICK_ENDDECLS
#endif

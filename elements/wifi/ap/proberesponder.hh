#ifndef CLICK_PROBERESPONDER_HH
#define CLICK_PROBERESPONDER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

ProbeResponder([, I<KEYWORDS>])

=s Wifi, Wireless AccessPoint

Respond to 802.11 probe packets.

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

class ProbeResponder : public Element { public:

  ProbeResponder() CLICK_COLD;
  ~ProbeResponder() CLICK_COLD;

  const char *class_name() const	{ return "ProbeResponder"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers() CLICK_COLD;

  void send_probe_response(EtherAddress);
  void push(int, Packet *);


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

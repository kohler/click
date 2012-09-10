#ifndef CLICK_OPENAUTHRESPONDER_HH
#define CLICK_OPENAUTHRESPONDER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

OpenAuthResponder([, I<KEYWORDS>])

=s Wifi, Wireless AccessPoint

Respond to 802.11 open authentication requests.

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

class OpenAuthResponder : public Element { public:

  OpenAuthResponder() CLICK_COLD;
  ~OpenAuthResponder() CLICK_COLD;

  const char *class_name() const	{ return "OpenAuthResponder"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers() CLICK_COLD;

  void send_auth_response(EtherAddress, uint16_t, uint16_t);
  void push(int, Packet *);


  bool _debug;

  class WirelessInfo *_winfo;
 private:


};

CLICK_ENDDECLS
#endif

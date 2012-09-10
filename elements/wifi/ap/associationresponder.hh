#ifndef CLICK_ASSOCIATIONRESPONDER_HH
#define CLICK_ASSOCIATIONRESPONDER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

AssociationResponder([, I<KEYWORDS>])

=s Wifi, Wireless AccessPoint

Respond to 802.11 association requests.

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
how often beacon packets are sent, in milliseconds.

=back 8

=a BeaconScanner

*/

class AssociationResponder : public Element { public:

  AssociationResponder() CLICK_COLD;
  ~AssociationResponder() CLICK_COLD;

  const char *class_name() const	{ return "AssociationResponder"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers() CLICK_COLD;

  void send_association_response(EtherAddress, uint16_t status, uint16_t associd);
  void recv_association_request(Packet *p);
  void send_disassociation(EtherAddress, uint16_t reason);
  void push(int, Packet *);


  bool _debug;

  uint16_t _associd;

  String scan_string();
  class AvailableRates *_rtable;
  class WirelessInfo *_winfo;
 private:


};

CLICK_ENDDECLS
#endif

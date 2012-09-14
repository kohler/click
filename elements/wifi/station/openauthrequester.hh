#ifndef CLICK_OPENAUTHREQUESTER_HH
#define CLICK_OPENAUTHREQUESTER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

OpenAuthRequeser

=s Wifi, Wireless Station

Sends 802.11 open authentication requests when poked.

=d

=h bssid read/write
The bssid to associate to

=h eth read/write
The station's ethernet address

=h ssid read/write
The ssid to associate to

=h listen_interval read/write
The listen interval for the station, in milliseconds

=h associated read only
If a association response was received and its status equals 0, this will
be true.

=h send_auth_req
Sends an authentication request based on values of the handlers.

=a BeaconScanner */

class OpenAuthRequester : public Element { public:

  OpenAuthRequester() CLICK_COLD;
  ~OpenAuthRequester() CLICK_COLD;

  const char *class_name() const	{ return "OpenAuthRequester"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers() CLICK_COLD;

  void send_auth_request();
  void push(int, Packet *);


  bool _debug;
  EtherAddress _eth;
  class WirelessInfo *_winfo;
 private:


};

CLICK_ENDDECLS
#endif

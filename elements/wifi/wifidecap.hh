#ifndef CLICK_WIFIDECAP_HH
#define CLICK_WIFIDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

WifiDecap

=s Wifi

Turns 802.11 packets into ethernet packets.

=d

Strips the 802.11 frame header and llc header off the packet and pushes
an ethernet header onto the packet. Discards packets that are shorter
than the length of the 802.11 frame header and llc header.

=over 8

=item ETHER

Boolean.  If true (this is the default), then push an Ethernet header onto the
packet after removing the wifi header.

=back

=e

FromDevice(ath0)
-> ExtraDecap()
-> FilterTX()
-> FilterPhyErr()
-> wifi_cl :: Classifier (0/08%0c); //data packets
-> wifi_decap :: WifiDecap()
-> HostEtherFilter(ath0, DROP_OTHER true, DROP_OWN true)
...
=a WifiEncap
 */

class WifiDecap : public Element { public:

  WifiDecap() CLICK_COLD;
  ~WifiDecap() CLICK_COLD;

  const char *class_name() const	{ return "WifiDecap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  bool _debug;
  bool _strict;
  bool _push_eth;
 private:

};

CLICK_ENDDECLS
#endif

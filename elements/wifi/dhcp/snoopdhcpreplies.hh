#ifndef CLICK_SNOOPDHCPREPLIES_HH
#define CLICK_SNOOPDHCPREPLIES_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

SnoopDHCPReplies

=s Wifi

Turns 802.11 packets into ethernet packets.

=d

Strips the 802.11 frame header and llc header off the packet and pushes
an ethernet header onto the packet. Discards packets that are shorter
than the length of the 802.11 frame header and llc header.


=e

FromDevice(ath0)
-> ExtraDecap()
-> FilterTX()
-> FilterPhyErr()
-> wifi_cl :: Classifier (0/08%0c); //data packets
-> wifi_decap :: SnoopDHCPReplies()
-> HostEtherFilter(ath0, DROP_OTHER true, DROP_OWN true)
...
=a WifiEncap
 */

class SnoopDHCPReplies : public Element { public:
  
  SnoopDHCPReplies();
  ~SnoopDHCPReplies();

  const char *class_name() const	{ return "SnoopDHCPReplies"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;
 private:

  String _ifname;
};

CLICK_ENDDECLS
#endif

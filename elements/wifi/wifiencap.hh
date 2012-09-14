#ifndef CLICK_WIFIENCAP_HH
#define CLICK_WIFIENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

WifiEncap(MODE, BSSID)

=s Wifi

Converts ethernet packets to 802.11 packets with a LLC header.

=d

Strips the ethernet header off the front of the packet and pushes
an 802.11 frame header and llc header onto the packet.

Arguments are:

=over 8
=item BSSID
is an ethernet address. This ususally the access point's ethernet address.
If you are using Mode 0, this is usually set to 00:00:00:00:00:00 for
"psuedo-ibss" mode.

=item MODE
This specifies which address field the BSSID field is located at (for
instance, the BSSID is the destination when MODE is 1, because the
packet is going to the access point).
It should be one of:
N<>0 station -> station
N<>1 station -> access point
N<>2 access point -> station
N<>3 access point -> access point

=back 8


=h mode read/write
Same as MODE argument

=h bssid read/write
Same as BSSID argument

=e

// this configuration sends 1000 broadcast packets at 1 megabit
// to device ath0 with ethertype 0x9000

inf_src :: InfiniteSource(DATA \<ffff>, LIMIT 1000, ACTIVE false)
-> EtherEncap(0x9000, ath0, ff:ff:ff:ff:ff:ff)
-> wifi_encap :: WifiEncap(0x00, 0:0:0:0:0:0)
-> set_rate :: SetTXRate(RATE 2)
-> ExtraEncap()
-> to_dev :: ToDevice (ath0);

=a EtherEncap */

class WifiEncap : public Element { public:

  WifiEncap() CLICK_COLD;
  ~WifiEncap() CLICK_COLD;

  const char *class_name() const	{ return "WifiEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  bool _debug;

  unsigned _mode;
  EtherAddress _bssid;
  class WirelessInfo *_winfo;
 private:

};

CLICK_ENDDECLS
#endif

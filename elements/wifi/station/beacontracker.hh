#ifndef CLICK_BEACONTRACKER_HH
#define CLICK_BEACONTRACKER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/deque.hh>
CLICK_DECLS

/*
=c

BeaconTracker

=s Wifi, Wireless Station

Tracks beacon from an Access Point

=d


=h stats read only
The percent of probes received.

=h reset read/write
Clear the list of access points.

 */

class BeaconTracker : public Element { public:

  BeaconTracker() CLICK_COLD;
  ~BeaconTracker() CLICK_COLD;

  const char *class_name() const	{ return "BeaconTracker"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

  struct beacon_t {
    Timestamp rx;
    uint16_t seq;
  };
  Deque<beacon_t> _beacons;

  int _beacon_int;
  Timestamp _start;

  void trim();
  void add_handlers() CLICK_COLD;
  void reset();

  bool _debug;

  int _track;

 private:

  class WirelessInfo *_winfo;
};

CLICK_ENDDECLS
#endif

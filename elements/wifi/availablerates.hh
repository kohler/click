#ifndef CLICK_AVAILABLERATES_HH
#define CLICK_AVAILABLERATES_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

AvailableRates()

=s Wifi, Wireless Station, Wireless AccessPoint

Tracks bit-rate capabilities of other stations.

=d

Tracks a list of bitrates other stations are capable of.

=h insert write-only
Inserts an ethernet address and a list of bitrates to the database.

=h remove write-only
Removes an ethernet address from the database.

=h rates read-only
Shows the entries in the database.

=a BeaconScanner
 */


class AvailableRates : public Element { public:

  AvailableRates() CLICK_COLD;
  ~AvailableRates() CLICK_COLD;

  const char *class_name() const		{ return "AvailableRates"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void *cast(const char *n);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers() CLICK_COLD;
  void take_state(Element *e, ErrorHandler *);

  Vector<int> lookup(EtherAddress eth);
  int insert(EtherAddress eth, Vector<int>);

  EtherAddress _bcast;
  bool _debug;

  int parse_and_insert(String s, ErrorHandler *errh);

  class DstInfo {
  public:
    EtherAddress _eth;
    Vector<int> _rates;
    DstInfo() {
      memset(this, 0, sizeof(*this));
    }

    DstInfo(EtherAddress eth) {
      memset(this, 0, sizeof(*this));
      _eth = eth;
    }
  };

  typedef HashMap<EtherAddress, DstInfo> RTable;
  typedef RTable::const_iterator RIter;

  RTable _rtable;
  Vector<int> _default_rates;
private:
};

CLICK_ENDDECLS
#endif

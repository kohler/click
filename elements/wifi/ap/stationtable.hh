#ifndef CLICK_STATIONTABLE_HH
#define CLICK_STATIONTABLE_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class Station {
public:
  EtherAddress _eth;
  Timestamp _when; // When we last heard from this node.
  Station() {
    memset(this, 0, sizeof(*this));
  }
};



class StationTable : public Element { public:

  StationTable() CLICK_COLD;
  ~StationTable() CLICK_COLD;

  const char *class_name() const		{ return "StationTable"; }
  const char *port_count() const		{ return PORTS_0_0; }

  void add_handlers() CLICK_COLD;
  void take_state(Element *e, ErrorHandler *);


  bool _debug;

private:

  typedef HashMap<EtherAddress, Station> STable;
  typedef STable::const_iterator STIter;

  STable _table;

};

CLICK_ENDDECLS
#endif

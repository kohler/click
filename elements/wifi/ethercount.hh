#ifndef CLICK_ETHERCOUNT_HH
#define CLICK_ETHERCOUNT_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

EtherCount

=s Wifi

Track  each ethernet source.

=d



=h stats
Print information accumulated for each source

=h reset
Clear all information for each source

=a

*/


class EtherCount : public Element { public:

  EtherCount() CLICK_COLD;
  ~EtherCount() CLICK_COLD;

  const char *class_name() const		{ return "EtherCount"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;

  class DstInfo {
  public:
    EtherAddress _eth;
    int count;

    DstInfo() {
      memset(this, 0, sizeof(*this));
    }

    DstInfo(EtherAddress eth) {
      memset(this, 0, sizeof(*this));
      _eth = eth;
    }

  };
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  NeighborTable _neighbors;

};

CLICK_ENDDECLS
#endif

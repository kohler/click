#ifndef CLICK_RXSTATS_HH
#define CLICK_RXSTATS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

RXStats

=s Wifi

Track RSSI for each ethernet source.

=d
Accumulate RSSI, noise for each ethernet source you hear a packet from.


=h stats
Print information accumulated for each source

=h reset
Clear all information for each source

=a ExtraDecap

*/


class RXStats : public Element { public:

  RXStats() CLICK_COLD;
  ~RXStats() CLICK_COLD;

  const char *class_name() const		{ return "RXStats"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;

  class DstInfo {
  public:
    EtherAddress _eth;
    int _rate;
    int _noise;
    int _signal;

    int _packets;
    unsigned _sum_signal;
    unsigned _sum_noise;
    Timestamp _last_received;

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
  EtherAddress _bcast;
  int _tau;


};

CLICK_ENDDECLS
#endif

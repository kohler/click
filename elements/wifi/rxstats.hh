#ifndef CLICK_RXSTATS_HH
#define CLICK_RXSTATS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * RXStats()
 * 
 * =s wifi
 * 
 * Accumulate rxstats for each ethernet src you hear a packet from.
 * =over 8
 *
 * =item TAU
 * 
 * Unsigned integer. Values 1 - 100 on  how much last packet
 * influences counters.
 *
 */


class RXStats : public Element { public:
  
  RXStats();
  ~RXStats();
  
  const char *class_name() const		{ return "RXStats"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  RXStats *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_stats(Element *e, void *);
  String print_stats();

  int get_rate(EtherAddress);
 private:

  
  class DstInfo {
  public:
    EtherAddress _eth;
    bool _rate_guessed;
    int _rate;
    int _noise;
    int _signal;
    struct timeval _last_received;
    
    DstInfo() { 
      memset(this, 0, sizeof(*this));
    }

    DstInfo(EtherAddress eth) { 
      memset(this, 0, sizeof(*this));
      _eth = eth;
    }

  };
  typedef BigHashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  int _tau;
};

CLICK_ENDDECLS
#endif

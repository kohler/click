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
 * Sets the Wifi TXRate Annotation on a packet.
 * =over 8
 *
 * =item RATE
 * 
 * Unsigned integer. Valid rates are 1, 2, 5, and 11.
 *
 * =item AUTO
 * 
 * Boolean. Use auto rate scaling. Default is false.
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

 private:

  
  class DstInfo {
  public:
    EtherAddress _eth;
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
};

CLICK_ENDDECLS
#endif

#ifndef CLICK_AUTOTXRATE_HH
#define CLICK_AUTOTXRATE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * AutoTXRate()
 * 
 * =s wifi
 * 
 * Automatically determine the txrate for a give ethernet dst
 * =over 8
 *
 * =item Probation Count
 * 
 * Unsigned int.  Number of packets before trying the next
 * highest rate.
 *
 * =a
 * SetTXRate, RXStats
 */


class AutoTXRate : public Element { public:
  
  AutoTXRate();
  ~AutoTXRate();
  
  const char *class_name() const		{ return "AutoTXRate"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  AutoTXRate *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_stats(Element *e, void *);
  String print_stats();

  int get_tx_rate(EtherAddress dst);
 private:

  
  class DstInfo {
  public:
    EtherAddress _eth;
    int _rate;
    int _packets;
    struct timeval _last_success;
    
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
  int _max_probation_count;
};

CLICK_ENDDECLS
#endif

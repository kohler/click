#ifndef CLICK_AUTOTXPOWER_HH
#define CLICK_AUTOTXPOWER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * AutoTXPower()
 * 
 * =s wifi
 * 
 * Sets the Wifi TXPower Annotation on a packet.
 * =over 8
 *
 * =item POWER
 * 
 * Unsigned integer. Valid powers are 1, 2, 5, and 11.
 *
 * =item AUTO
 * 
 * Boolean. Use auto power scaling. Default is false.
 *
 */


class AutoTXPower : public Element { public:
  
  AutoTXPower();
  ~AutoTXPower();
  
  const char *class_name() const		{ return "AutoTXPower"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_stats(Element *e, void *);
  String print_stats();

  int get_tx_power(EtherAddress dst);
 private:

  
  class DstInfo {
  public:
    EtherAddress _eth;
    int _power;
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
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  int _max_probation_count;
};

CLICK_ENDDECLS
#endif

#ifndef CLICK_AVAILABLERATES_HH
#define CLICK_AVAILABLERATES_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * AvailableRates()
 * 
 *
 */


class AvailableRates : public Element { public:
  
  AvailableRates();
  ~AvailableRates();
  
  const char *class_name() const		{ return "AvailableRates"; }

  int configure(Vector<String> &, ErrorHandler *);
  void *cast(const char *n);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();
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

#ifndef CLICK_ARPTABLE_HH
#define CLICK_ARPTABLE_HH
#include <click/element.hh>
#include <click/ip6address.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * ARPTable()
 * 
 *
 */


class ARPTable : public Element { public:
  
  ARPTable();
  ~ARPTable();
  
  const char *class_name() const		{ return "ARPTable"; }

  ARPTable *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  void *cast(const char *n);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  static String static_print_mappings(Element *e, void *);
  String print_mappings();


  /* returns ff:ff:ff:ff:ff:ff if none is found */
  EtherAddress lookup(IP6Address ip);

  void insert(IP6Address ip, EtherAddress eth);
 private:
  
  // Poor man's ARP cache. 
  class DstInfo {
  public:
    IP6Address _ip;
    EtherAddress _eth;
    struct timeval _when; // When we last heard from this node.
    DstInfo() { 
      memset(this, 0, sizeof(*this));
    }

    DstInfo(IP6Address ip) { 
      memset(this, 0, sizeof(*this));
      _ip = ip;
    }
  };
  
  typedef BigHashMap<IP6Address, DstInfo> ATable;
  typedef ATable::const_iterator ARPIter;
  
  ATable _table;

  EtherAddress _bcast;
  
};

CLICK_ENDDECLS
#endif

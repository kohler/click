#ifndef CLICK_ARPTABLE_HH
#define CLICK_ARPTABLE_HH
#include <click/element.hh>
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
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  static String static_print_mappings(Element *e, void *);
  String print_mappings();


  /* returns ff:ff:ff:ff:ff:ff if none is found */
  EtherAddress lookup(IPAddress ip);

  void insert(IPAddress ip, EtherAddress eth);
 private:
  
  // Poor man's ARP cache. 
  class DstInfo {
  public:
    IPAddress _ip;
    EtherAddress _eth;
    struct timeval _when; // When we last heard from this node.
    DstInfo() { 
      memset(this, 0, sizeof(*this));
    }
  };
  
  typedef BigHashMap<IPAddress, DstInfo> ATable;
  typedef ATable::const_iterator ARPIter;
  
  ATable _table;

  EtherAddress _bcast;
  
};

CLICK_ENDDECLS
#endif

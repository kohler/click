#ifndef CLICK_ARPTABLE_HH
#define CLICK_ARPTABLE_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
ARPTable

=s Wifi 

Tracks IP -> Ethernet mappings for other elements.

=d 
Tracks ethernet to ip mappings for individual elements. This element
does not process packets.

=h mappings read-only
Returns a String with "EtherAddress IP" mappings on each line
=h insert write-only
Insert a "EtherAddress IP" pair into the table

*/


class ARPTable : public Element { public:
  
  ARPTable();
  ~ARPTable();
  
  const char *class_name() const		{ return "ARPTable"; }

  int configure(Vector<String> &, ErrorHandler *);
  void *cast(const char *n);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();
  void take_state(Element *e, ErrorHandler *);
  static String static_print_mappings(Element *e, void *);
  String print_mappings();
  static int static_insert(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  /* returns ff:ff:ff:ff:ff:ff if none is found */
  EtherAddress lookup(IPAddress ip);

  IPAddress reverse_lookup(EtherAddress eth);

  int insert(IPAddress ip, EtherAddress eth);
  EtherAddress _bcast;
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

    DstInfo(IPAddress ip) { 
      memset(this, 0, sizeof(*this));
      _ip = ip;
    }
  };
  
  typedef HashMap<IPAddress, DstInfo> ATable;
  typedef HashMap<EtherAddress, IPAddress> RTable;
  typedef ATable::const_iterator ARPIter;
  
  ATable _table;
  RTable _rev_table;
};

CLICK_ENDDECLS
#endif

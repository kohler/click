#ifndef CLICK_LINKTABLE_HH
#define CLICK_LINKTABLE_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
CLICK_DECLS


class IPPair {
public:
    /* a is always numerically less than b */
  IPAddress _a;
  IPAddress _b;

  IPPair() : _a(0), _b(0) { }

  IPPair(IPAddress a, IPAddress b) {
    if (a.addr() < b.addr()) {
      _a = b;
      _b = a;      
    } else {
      _a = a;
      _b = b;
    }
  }
  
  bool contains(IPAddress foo) {
    return ((foo == _a) || (foo == _b));
  }
  bool other(IPAddress foo) { return ((_a == foo) ? _b : _a); }


  inline bool
  operator==(IPPair other)
  {
    return (other._a == _a && other._b == _b);
  }

};

  inline unsigned
  hashcode(IPPair p) 
  {
    return hashcode(p._a) + hashcode(p._b);
  }





class LinkTable: public Element{
public: 

  /* generic click-mandated stuff*/
  LinkTable();
  ~LinkTable();
  void add_handlers();
  const char* class_name() const { return "LinkTable"; }
  LinkTable *clone() const { return new LinkTable(); }
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* read/write handlers */
  static String static_print_routes(Element *e, void *);
  String print_routes();
  static String static_print_links(Element *e, void *);
  String print_links();
  static String static_print_hosts(Element *e, void *);
  String print_hosts();
  static String static_print_neighbors(Element *e, void *);
  String print_neighbors();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  /* other public functions */
  void update_link(IPPair p, u_short metric, unsigned int now);
  u_short get_hop_metric(IPPair p);
  u_short get_route_metric(Vector<IPAddress> route, int size);
  void dijkstra();
  Vector<IPAddress> best_route(IPAddress dst);
  u_short get_host_metric(IPAddress s);
  Vector<IPAddress> get_hosts();
private: 
  class LinkInfo {
  public:
    IPPair _p;
    u_short _metric;
    unsigned int _last_updated;
    LinkInfo() { _p = IPPair();  _metric = 9999;  _last_updated = 0; }
    
    LinkInfo(IPPair p, u_short metric, unsigned int now)
    { 
      _metric = metric;
      _p = IPPair(p._a, p._b);
      _last_updated = now;  
    }
    
    void update(u_short metric, unsigned int now) { _metric = metric; _last_updated = now; }
    bool contains(IPAddress foo) { return _p.contains(foo); }
    IPPair get_pair() { return _p; }
  };

  typedef HashMap<IPAddress, IPAddress> IPTable;
  
  class HostInfo {
  public:
    IPAddress _ip;
    IPTable _neighbors;
    u_short _metric;
    IPAddress _prev;
    bool _marked;
    HostInfo() { _ip = IPAddress(0); _prev = IPAddress(0); _metric = 9999; _marked = false;}
    HostInfo(IPAddress p) { _ip = p; _prev = IPAddress(0); _metric = 9999; _marked = false; }
    void clear() { _prev = IPAddress(0); _metric = 9999; _marked = false;}

  };

  typedef BigHashMap<IPAddress, HostInfo> HTable;
  typedef HTable::const_iterator HTIter;
  

  typedef BigHashMap<IPPair, LinkInfo> LTable;
  typedef LTable::const_iterator LTIter;

  class HTable _hosts;
  class LTable _links;

  IPAddress _ip;

  IPAddress extract_min();
  void lt_assert_(const char *, int, const char *) const;
};
  


CLICK_ENDDECLS
#endif /* CLICK_LINKTABLE_HH */





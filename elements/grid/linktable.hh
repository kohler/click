#ifndef CLICK_LINKTABLE_HH
#define CLICK_LINKTABLE_HH
#include <click/ip6address.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
CLICK_DECLS


class IP6Pair {
public:

  IP6Address _to;
  IP6Address _from;

  IP6Pair() : _to(), _from() { }

  IP6Pair(IP6Address to, IP6Address from) {
      _to = to;
      _from = from;
  }

  bool contains(IP6Address foo) {
    return ((foo == _to) || (foo == _from));
  }
  bool other(IP6Address foo) { return ((_to == foo) ? _from : _to); }


  inline bool
  operator==(IP6Pair other)
  {
    return (other._to == _to && other._from == _from);
  }

};

inline unsigned
hashcode(IP6Pair p) 
{
return hashcode(p._to) + hashcode(p._from);
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
  void *cast(const char *n);
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

  static int static_update_link(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  static int static_dijkstra(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 

  /* other public functions */
  void update_link(IP6Address from, IP6Address to, int metric);
  int get_hop_metric(IP6Address from, IP6Address to);
  Vector< Vector<IP6Address> >  update_routes(Vector<Vector<IP6Address> > routes, 
					     int n, Vector<IP6Address> route);
  bool valid_route(Vector<IP6Address> route);
  int get_route_metric(Vector<IP6Address> route);
  void dijkstra();
  Vector<IP6Address> best_route(IP6Address dst);

  Vector< Vector<IP6Address> > top_n_routes(IP6Address dst, int n);
  int get_host_metric(IP6Address s);
  Vector<IP6Address> get_hosts();
private: 
  class LinkInfo {
  public:
    IP6Address _from;
    IP6Address _to;
    int _metric;
    struct timeval _last_updated;
    LinkInfo() { _from = IP6Address(); _to = IP6Address(); _metric = 0; _last_updated.tv_sec = 0; }
    
    LinkInfo(IP6Address from, IP6Address to, int metric)
    { 
      _from = from;
      _to = to;
      _metric = metric;
      click_gettimeofday(&_last_updated);
    }
    LinkInfo(const LinkInfo &p) : _from(p._from), _to(p._to), _metric(p._metric), _last_updated(p._last_updated) { }
    void update(int metric) { 
      _metric = metric; 
      click_gettimeofday(&_last_updated); 
    }
    
  };

  typedef HashMap<IP6Address, IP6Address> IPTable;
  
  class HostInfo {
  public:
    IP6Address _ip;
    IPTable _neighbors;
    int _metric;
    IP6Address _prev;
    bool _marked;
    HostInfo() { _ip = IP6Address(); _prev = IP6Address(); _metric = 0; _marked = false;}
    HostInfo(IP6Address p) { _ip = p; _prev = IP6Address(); _metric = 0; _marked = false; }
    HostInfo(const HostInfo &p) : _ip(p._ip), _neighbors(p._neighbors), _metric(p._metric), _prev(p._prev), _marked(p._marked) { }
    void clear() { _prev = IP6Address(); _metric = 0; _marked = false;}

  };

  typedef BigHashMap<IP6Address, HostInfo> HTable;
  typedef HTable::const_iterator HTIter;
  

  typedef BigHashMap<IP6Pair, LinkInfo> LTable;
  typedef LTable::const_iterator LTIter;

  class HTable _hosts;
  class LTable _links;

  IP6Address _ip;

  IP6Address extract_min();
  static void _lt_assert_(const char *, int, const char *);
};
  


CLICK_ENDDECLS
#endif /* CLICK_LINKTABLE_HH */





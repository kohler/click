#ifndef CLICK_LINKTABLE_HH
#define CLICK_LINKTABLE_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
 * =c
 * LinkTable(IP Address, [STALE timeout])
 * =d
 * 
 *
 */
class IPPair {
public:

  IPAddress _to;
  IPAddress _from;

  IPPair() : _to(), _from() { }

  IPPair(IPAddress from, IPAddress to) {
      _to = to;
      _from = from;
  }

  bool contains(IPAddress foo) {
    return ((foo == _to) || (foo == _from));
  }
  bool other(IPAddress foo) { return ((_to == foo) ? _from : _to); }


  inline bool
  operator==(IPPair other)
  {
    return (other._to == _to && other._from == _from);
  }

};

inline unsigned
hashcode(IPPair p) 
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
  int initialize(ErrorHandler *);
  void run_timer();
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void *cast(const char *n);
  /* read/write handlers */
  static String static_print_routes(Element *e, void *);
  String print_routes();
  static String static_print_links(Element *e, void *);
  String print_links();
  static String static_print_hosts(Element *e, void *);
  String print_hosts();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  static int static_top_n_routes(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  static int static_update_link(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  static int static_dijkstra(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 

  String routes_to_string(Vector< Vector<IPAddress> > routes);
  /* other public functions */
  void update_link(IPAddress from, IPAddress to, int metric);
  void update_both_links(IPAddress a, IPAddress b, int metric) {
    update_link(a,b,metric);
    update_link(b,a,metric);
  }

  int get_hop_metric(IPAddress from, IPAddress to);
  Vector< Vector<IPAddress> >  update_routes(Vector<Vector<IPAddress> > routes, 
					     int n, Vector<IPAddress> route);
  bool valid_route(Vector<IPAddress> route);
  int get_route_metric(Vector<IPAddress> route);
  void dijkstra();
  void clear_stale();
  Vector<IPAddress> best_route(IPAddress dst);

  Vector< Vector<IPAddress> > top_n_routes(IPAddress dst, int n);
  int get_host_metric(IPAddress s);
  Vector<IPAddress> get_hosts();

  class Link {
  public:
    IPAddress _from;
    IPAddress _to;
    int _metric;
    Link() : _from(), _to(), _metric(0) { }
    Link(IPAddress from, IPAddress to, int metric) {
      _from = from;
      _to = to;
      _metric = metric;
    }
  };

  Link random_link();

private: 
  class LinkInfo {
  public:
    IPAddress _from;
    IPAddress _to;
    int _metric;
    struct timeval _last_updated;
    LinkInfo() { _from = IPAddress(); _to = IPAddress(); _metric = 0; _last_updated.tv_sec = 0; }
    
    LinkInfo(IPAddress from, IPAddress to, int metric)
    { 
      _from = from;
      _to = to;
      _metric = metric;
      click_gettimeofday(&_last_updated);
    }
    LinkInfo(const LinkInfo &p) : _from(p._from), _to(p._to), _metric(p._metric), _last_updated(p._last_updated) { }
    void update(int metric) {
      if (9999 == _metric) {
	/* once a link is marked as bad, 
	 * don't let anyone change it for
	 * at least two minutes
	 */
	struct timeval now;
	struct timeval diff;
	struct timeval expire;
	expire.tv_sec = 120;
	click_gettimeofday(&now);
	timersub(&now, &_last_updated, &diff);
	if (timercmp(&diff, &expire, <)) {
	  return;
	}
      }
      _metric = metric; 
      click_gettimeofday(&_last_updated); 
    }
    
  };

  typedef HashMap<IPAddress, IPAddress> IPTable;
  
  class HostInfo {
  public:
    IPAddress _ip;
    int _metric;
    IPAddress _prev;
    bool _marked;
    HostInfo() { _ip = IPAddress(); _prev = IPAddress(); _metric = 0; _marked = false;}
    HostInfo(IPAddress p) { _ip = p; _prev = IPAddress(); _metric = 0; _marked = false; }
    HostInfo(const HostInfo &p) : _ip(p._ip), _metric(p._metric), _prev(p._prev), _marked(p._marked) { }
    void clear() { _prev = IPAddress(); _metric = 0; _marked = false;}

  };

  typedef HashMap<IPAddress, HostInfo> HTable;
  typedef HTable::const_iterator HTIter;
  

  typedef HashMap<IPPair, LinkInfo> LTable;
  typedef LTable::const_iterator LTIter;

  class HTable _hosts;
  class LTable _links;

  IPAddress _ip;
  struct timeval _stale_timeout;
  Timer _timer;
  static void _lt_assert_(const char *, int, const char *);
};
  


CLICK_ENDDECLS
#endif /* CLICK_LINKTABLE_HH */





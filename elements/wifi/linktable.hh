#ifndef CLICK_LINKTABLE_HH
#define CLICK_LINKTABLE_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include "path.hh"
CLICK_DECLS

/*
 * =c
 * LinkTable(IP Address, [STALE timeout])
 * =s Wifi
 * Keeps a Link state database and calculates Weighted Shortest Path
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 * =a ARPTable
 *
 */
class IPPair {
  public:

    IPAddress _to;
    IPAddress _from;

    IPPair()
	: _to(), _from() {
    }

    IPPair(IPAddress from, IPAddress to)
	: _to(to), _from(from) {
    }

    bool contains(IPAddress foo) const {
	return (foo == _to) || (foo == _from);
    }

    bool other(IPAddress foo) const {
	return (_to == foo) ? _from : _to;
    }

    inline hashcode_t hashcode() const {
	return CLICK_NAME(hashcode)(_to) + CLICK_NAME(hashcode)(_from);
    }

    inline bool operator==(IPPair other) const {
	return (other._to == _to && other._from == _from);
    }

};


class LinkTable: public Element{
public:

  /* generic click-mandated stuff*/
  LinkTable() CLICK_COLD;
  ~LinkTable() CLICK_COLD;
  void add_handlers() CLICK_COLD;
  const char* class_name() const { return "LinkTable"; }
  int initialize(ErrorHandler *) CLICK_COLD;
  void run_timer(Timer *);
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
  void take_state(Element *, ErrorHandler *);
  void *cast(const char *n);
  /* read/write handlers */
  String print_routes(bool, bool);
  String print_links();
  String print_hosts();

  static int static_update_link(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  void clear();

  /* other public functions */
  String route_to_string(Path p);
  bool update_link(IPAddress from, IPAddress to,
		   uint32_t seq, uint32_t age, uint32_t metric);
  bool update_both_links(IPAddress a, IPAddress b,
			 uint32_t seq, uint32_t age, uint32_t metric) {
    if (update_link(a,b,seq,age, metric)) {
      return update_link(b,a,seq,age, metric);
    }
    return false;
  }

  uint32_t get_link_metric(IPAddress from, IPAddress to);
  uint32_t get_link_seq(IPAddress from, IPAddress to);
  uint32_t get_link_age(IPAddress from, IPAddress to);
  bool valid_route(const Vector<IPAddress> &route);
  unsigned get_route_metric(const Vector<IPAddress> &route);
  Vector<IPAddress> get_neighbors(IPAddress ip);
  void dijkstra(bool);
  void clear_stale();
  Vector<IPAddress> best_route(IPAddress dst, bool from_me);

  Vector< Vector<IPAddress> > top_n_routes(IPAddress dst, int n);
  uint32_t get_host_metric_to_me(IPAddress s);
  uint32_t get_host_metric_from_me(IPAddress s);
  Vector<IPAddress> get_hosts();

  class Link {
  public:
    IPAddress _from;
    IPAddress _to;
    uint32_t _seq;
    uint32_t _metric;
    Link() : _from(), _to(), _seq(0), _metric(0) { }
    Link(IPAddress from, IPAddress to, uint32_t seq, uint32_t metric) {
      _from = from;
      _to = to;
      _seq = seq;
      _metric = metric;
    }
  };

  Link random_link();


  typedef HashMap<IPAddress, IPAddress> IPTable;
  typedef IPTable::const_iterator IPIter;

  IPTable _blacklist;

  Timestamp dijkstra_time;
protected:
  class LinkInfo {
  public:
    IPAddress _from;
    IPAddress _to;
    unsigned _metric;
    uint32_t _seq;
    uint32_t _age;
    Timestamp _last_updated;
    LinkInfo() {
      _from = IPAddress();
      _to = IPAddress();
      _metric = 0;
      _seq = 0;
      _age = 0;
    }

    LinkInfo(IPAddress from, IPAddress to,
	     uint32_t seq, uint32_t age, unsigned metric) {
      _from = from;
      _to = to;
      _metric = metric;
      _seq = seq;
      _age = age;
      _last_updated.assign_now();
    }

    LinkInfo(const LinkInfo &p) :
      _from(p._from), _to(p._to),
      _metric(p._metric), _seq(p._seq),
      _age(p._age),
      _last_updated(p._last_updated)
    { }

    uint32_t age() {
	Timestamp now = Timestamp::now();
	return _age + (now.sec() - _last_updated.sec());
    }
    void update(uint32_t seq, uint32_t age, unsigned metric) {
      if (seq <= _seq) {
	return;
      }
      _metric = metric;
      _seq = seq;
      _age = age;
      _last_updated.assign_now();
    }

  };

  class HostInfo {
  public:
    IPAddress _ip;
    uint32_t _metric_from_me;
    uint32_t _metric_to_me;

    IPAddress _prev_from_me;
    IPAddress _prev_to_me;

    bool _marked_from_me;
    bool _marked_to_me;

    HostInfo(IPAddress p = IPAddress()) {
      _ip = p;
      _metric_from_me = 0;
      _metric_to_me = 0;
      _prev_from_me = IPAddress();
      _prev_to_me = IPAddress();
      _marked_from_me = false;
      _marked_to_me = false;
    }

    HostInfo(const HostInfo &p) :
      _ip(p._ip),
      _metric_from_me(p._metric_from_me),
      _metric_to_me(p._metric_to_me),
      _prev_from_me(p._prev_from_me),
      _prev_to_me(p._prev_to_me),
      _marked_from_me(p._marked_from_me),
      _marked_to_me(p._marked_to_me)
    { }

    void clear(bool from_me) {
      if (from_me ) {
	_prev_from_me = IPAddress();
	_metric_from_me = 0;
	_marked_from_me = false;
      } else {
	_prev_to_me = IPAddress();
	_metric_to_me = 0;
	_marked_to_me = false;
      }
    }

  };

  typedef HashMap<IPAddress, HostInfo> HTable;
  typedef HTable::const_iterator HTIter;


  typedef HashMap<IPPair, LinkInfo> LTable;
  typedef LTable::const_iterator LTIter;

  HTable _hosts;
  LTable _links;


  IPAddress _ip;
  Timestamp _stale_timeout;
  Timer _timer;
};



CLICK_ENDDECLS
#endif /* CLICK_LINKTABLE_HH */

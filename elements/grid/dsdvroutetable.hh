#ifndef CLICK_DSDVROUTETABLE_HH
#define CLICK_DSDVROUTETABLE_HH
#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <elements/grid/grid.hh>
#include <elements/grid/gridgenericrt.hh>
#include <click/timer.hh>
#include <elements/grid/gridgenericlogger.hh>
CLICK_DECLS

/*
 * =c
 * DSDVRouteTable(TIMEOUT, PERIOD, JITTER, MIN_TRIGGER_PERIOD, ETH, IP [, I<KEYWORDS>])
 *
 * =s Grid
 * Run DSDV local routing protocol
 *
 * =d 
 *
 * This is meant to be an ``official'' implementation of DSDV.  It is
 * intended to exactly replicate the behavior of DSDV as described in
 * the original Perkins paper, the CMU ad-hoc bakeoff paper, and the
 * CMU ns implementation.  I make no guarantees that any of the above
 * are actually achieved.
 *
 * DSDVRouteTable expects the Paint annotation to be set on Grid
 * packets arriving from the network.
 *
 * There must only be one of DSDVRouteTable or GridRouteTable in a
 * grid configuration.
 *
 * Regular arguments are:
 *
 * =over 8
 *
 * =item TIMEOUT
 * 
 * Unsigned integer.  Milliseconds after which to expire route entries.
 *
 * =item PERIOD
 *
 * Unsigned integer.  Milliseconds between full dumps, plus/minus some jitter (see below).
 *
 * =item JITTER
 *
 * Unsigned integer.  Maximum milliseconds by which to randomly jitter full dumps.
 *
 * =item MIN_TRIGGER_PERIOD
 *
 * Unsigned integer.  Minimum milliseconds between triggered updates.
 *
 * =item ETH
 *
 * This node's ethernet hardware address.
 *
 * =item IP
 *
 * This node's IP address.
 *
 * =back
 *
 * Keywords arguments are:
 *
 * =over 8
 *
 * =item GW
 *
 * GridGatewayInfo element.  Determines whether or not this node is a
 * gateway.  If this argument is not provided, the node is not a
 * gateway.
 *
 * =item LS
 *
 * LinkStat element.  Required to use link metrics other than
 * hopcount.
 *
 * =item MAX_HOPS
 * 
 * Unsigned integer.  The maximum number of hops for which a route
 * should propagate.  The default number of hops is 3.
 *
 * =item METRIC
 *
 * String.  The type of metric that should be used to compare two
 * routes.  Allowable values are: ``hopcount'', ``est_tx_count''
 * (estimated transmission count), ``delivery_rate_product'', or
 * ``reverse_delivery_rate_product''.  The default is to use hopcount.
 *
 * =item LOG
 *
 * GridGenericLogger element.  Object to log Grid events to.
 *
 * =item WST0 (zero, not ``oh'')
 * 
 * Unsigned integer.  Initial weighted settling time.  Milliseconds.
 *
 * =item ALPHA
 * 
 * Unsigned integer.  DSDV settling time weighting parameter, in percent.  Between 0 and 100 inclusive.
 *
 * =item SEQ0 (zero, not ``oh'')
 *
 * Unsigned integer.  Initial sequence number used in advertisements.
 * Defaults to 0.  Must be even.
 *
 * =a
 * SendGridHello, FixSrcLoc, SetGridChecksum, LookupLocalGridRoute, LookupGeographicGridRoute
 * GridGatewayInfo, LinkStat, LinkTracker, GridRouteTable, GridLogger, Paint */

class GridGatewayInfo;
class LinkStat;

class DSDVRouteTable : public GridGenericRouteTable {

public:
  // generic rt methods
  bool current_gateway(RouteEntry &entry);
  bool get_one_entry(IPAddress &dest_ip, RouteEntry &entry);
  void get_all_entries(Vector<RouteEntry> &vec);
 
  DSDVRouteTable();
  ~DSDVRouteTable();

  const char *class_name() const		{ return "DSDVRouteTable"; }
  void *cast(const char *);
  const char *processing() const		{ return "h/h"; }
  const char *flow_code() const                 { return "x/y"; }
  DSDVRouteTable *clone() const                 { return new DSDVRouteTable; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  virtual bool can_live_reconfigure() const { return false; }

  Packet *simple_action(Packet *);

  void add_handlers();
  

private:

  struct metric_t {
    static const unsigned int bad_metric = 777777;    
    bool valid;
    unsigned int val;
    metric_t() : valid(false), val(bad_metric) { }
    metric_t(const metric_t &m) : valid(m.valid), val(m.val) { }
    metric_t(unsigned int m, bool v = true) : valid(v), val(m) { }
  };
  
  /* 
   * route table entry
   */
  class RTEntry : public RouteEntry {
  private:
    bool                _init;

  public:
    class EtherAddress  dest_eth;              // hardware address of destination; 
                                               // may be all 0s if we don't hear any ads...
    bool                is_gateway;

    unsigned int        ttl;                   // msecs
    unsigned int        last_updated_jiffies;  // last time this entry was updated

    // metrics are invalid until updated to incorporate the last hop's
    // link, i.e. by calling initialize_metric or update_metric.
    metric_t            metric;


    // DSDV book-keeping
    unsigned int        wst;                   // weighted settling time (msecs)
    metric_t            last_adv_metric;       // last metric we advertised
    unsigned int        last_seq_jiffies;      // last time the seq_no changed
    unsigned int        advertise_ok_jiffies;  // when it is ok to advertise route
    bool                need_seq_ad;
    bool                need_metric_ad;
    unsigned int        last_expired_jiffies;  // when the route was expired (if broken)

    bool broken() const { check(); return num_hops() == 0; }
    bool good()   const { check(); return num_hops() != 0; }

    String dump()  const; 
    void   check() const { 
      assert(_init); 
      assert((num_hops() > 0) != (seq_no() & 1)); 
      // only check if last_seq_jiff has been set
      assert(last_seq_jiffies ? last_updated_jiffies >= last_seq_jiffies : true); 
    } 

    void invalidate(unsigned int jiff) {
      check();
      assert(num_hops() > 0);
      _num_hops = 0;
      _seq_no++;
      last_expired_jiffies = jiff;
      check();
    }

    RTEntry() : 
      _init(false), is_gateway(false), ttl(0), last_updated_jiffies(0), wst(0), 
      last_seq_jiffies(0), advertise_ok_jiffies(0), need_seq_ad(false), 
      need_metric_ad(false), last_expired_jiffies(0)
    { }

    /* constructor for 1-hop route entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_hdr *gh, grid_hello *hlo,
	    unsigned char interface, unsigned int jiff) :
      RouteEntry(ip, gh->loc_good, gh->loc_err, gh->loc, eth, ip, interface, hlo->seq_no, 1),
      _init(true), dest_eth(eth), is_gateway(hlo->is_gateway), ttl(hlo->ttl), last_updated_jiffies(jiff), 
      wst(0), last_seq_jiffies(jiff), advertise_ok_jiffies(0), need_seq_ad(false), 
      need_metric_ad(false), last_expired_jiffies(0)
    { 
      loc_err = ntohs(loc_err); 
      _seq_no = ntohl(_seq_no); 
      ttl = ntohl(ttl);
      check();
    }

    /* constructor from grid_nbr_entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_nbr_entry *nbr, 
	    unsigned char interface, unsigned int jiff) :
      RouteEntry(nbr->ip, nbr->loc_good, nbr->loc_err, nbr->loc,
		 eth, ip, interface, 
		 nbr->seq_no, nbr->num_hops > 0 ? nbr->num_hops + 1 : 0),
      _init(true), is_gateway(nbr->is_gateway), ttl(nbr->ttl), last_updated_jiffies(jiff), 
      metric(nbr->metric, nbr->metric_valid), wst(0), last_seq_jiffies(0), 
      advertise_ok_jiffies(0), need_seq_ad(false), need_metric_ad(false),
      last_expired_jiffies(nbr->num_hops > 0 ? 0 : jiff)
    {
      loc_err = ntohs(loc_err);
      _seq_no = ntohl(_seq_no);
      ttl = ntohl(ttl);
      metric.val = ntohl(metric.val);
      check();
    }
    
    /* copy data from this into nb, converting to net byte order */
    void fill_in(grid_nbr_entry *nb, LinkStat *ls = 0) const;
    
  };
  
  friend class RTEntry;
  
  typedef BigHashMap<IPAddress, RTEntry> RTable;
  typedef RTable::const_iterator RTIter;
  
  /* the route table */
  // Invariants: 

  // 1. every route in the table that is not expired (num_hops > 0) is
  // valid: i.e. its ttl has not run out, nor has it been in the table
  // past its timeout.  There is an expire timer for this route in
  // _expire_timers.

  // 2. no route in the table that *is* expired (num_hops == 0) has an
  // entry in _expire_timers.

  // 3. expired routes *are* allowed in the table, since that's what
  // the DSDV description does.

  class RTable _rtes;

  void handle_update(RTEntry &, const bool was_sender, const unsigned int jiff);  
  void insert_route(const RTEntry &, const GridGenericLogger::reason_t why);
  void schedule_triggered_update(const IPAddress &ip, unsigned int when); // when is in jiffies
  
  typedef BigHashMap<IPAddress, Timer *> TMap;
  typedef TMap::iterator TMIter;

  struct HookPair {
    DSDVRouteTable *obj;
    unsigned int ip;
    HookPair(DSDVRouteTable *o, unsigned int _ip) : obj(o), ip(_ip) { }
  private:
    HookPair() { }
  };

  typedef BigHashMap<IPAddress, HookPair *> HMap;
  typedef HMap::iterator HMIter;

  // Expire timer map invariants: every good route (r.good() is true)
  // has a running timer in _expire_timers.  No broken routes have a
  // timer.  Every entry in _expire_timers has a corresponding
  // TimerHook pointer stored in _expire_hooks.
  class TMap _expire_timers;
  class HMap _expire_hooks;

  // Trigger timer invariants: any route may have a timer in this
  // table.  All timers in the table must be running.  Every entry in
  // _trigger_timers has a corresponding TimerHook pointer stored in
  // _trigger_hooks.  Note: a route entry r may have a trigger timer
  // even if r.need_seq_ad and r.need_metric_ad flags are false.  We
  // might have sent a full update before r's triggered update timer
  // expired, but after r.advertise_ok_jiffies: the triggered update
  // was delayed past r.advertise_ok_jiffies to enforce the minimum
  // triggered update period.  In that case there may be other
  // destinations whose triggered updates were cancelled when r's
  // trigger was delayed, and which should be advertised when r's
  // triggered update timer finally fires, even if r needn't be.
  class TMap _trigger_timers;
  class HMap _trigger_hooks;

  // check table, timer, and trigger hook invariants
  void check_invariants(const IPAddress *ignore = 0) const; 

  /* max time to keep an entry in RT */
  unsigned int _timeout; // msecs

  /* route broadcast timing parameters */
  unsigned int _period; // msecs
  unsigned int _jitter; // msecs
  unsigned int _min_triggered_update_period; // msecs


  class GridGatewayInfo *_gw_info;
  class LinkStat        *_link_stat;

  /* binary logging */
  class GridGenericLogger  *_log;
  static const unsigned int _log_dump_period = 5 * 1000; // msecs

  /* this node's addresses */
  IPAddress _ip;
  EtherAddress _eth;

  /* latest sequence number for this node's route entry */
  unsigned int _seq_no;
  unsigned int _bcast_count;

  /* local DSDV radius */
  unsigned int _max_hops;

  /* settling time constants */
  unsigned int    _alpha; // percent, 0-100
  unsigned int    _wst0;  // msecs

  /* track route ads */
  unsigned int _last_periodic_update;  // jiffies
  unsigned int _last_triggered_update; // jiffies

  class Timer _hello_timer;
  static void static_hello_hook(Timer *, void *e) { ((DSDVRouteTable *) e)->hello_hook(); }
  void hello_hook();

  class Timer _log_dump_timer;
  static void static_log_dump_hook(Timer *, void *e) { ((DSDVRouteTable *) e)->log_dump_hook(true); }
  void log_dump_hook(bool reschedule);


  static void static_expire_hook(Timer *, void *v) 
  { ((HookPair *) v)->obj->expire_hook(((HookPair *) v)->ip); }

  void expire_hook(const IPAddress &);

  static void static_trigger_hook(Timer *, void *v)
  { ((HookPair *) v)->obj->trigger_hook(((HookPair *) v)->ip); }

  void trigger_hook(const IPAddress &);

  void send_full_update();
  void send_triggered_update(const IPAddress &);

  /* send a route advertisement containing the specified entries */
  void build_and_tx_ad(Vector<RTEntry> &);

public:
  static unsigned int decr_ttl(unsigned int ttl, unsigned int decr)
  { return (ttl > decr ? ttl - decr : 0); }

  static unsigned int jiff_to_msec(unsigned int j)
  { return (j * 1000) / CLICK_HZ; }

  static unsigned int msec_to_jiff(unsigned int m)
  { return (CLICK_HZ * m) / 1000; }

private:
  class RouteEntry make_generic_rte(const RTEntry &rte) { return rte; }

  /* update route metric with the last hop from the advertising node */
  void update_metric(RTEntry &);

  /* initialize the metric for a 1-hop neighbor */
  void init_metric(RTEntry &);

  /* update weight settling time */
  void update_wst(RTEntry *old_r, RTEntry &new_r, const unsigned int jiff);

  /* true iff first route's metric is preferable to second route's
     metric -- note that this is a strict comparison, if the metrics
     are equivalent, then the function returns false.  this is
     necessary to get stability, e.g. when using the hopcount metric */
  bool metric_preferable(const RTEntry &, const RTEntry &);
  bool metrics_differ(const metric_t &, const metric_t &);
  bool metric_val_lt(unsigned int, unsigned int);

  static String print_rtes(Element *e, void *);
  static String print_rtes_v(Element *e, void *);
  static String print_nbrs(Element *e, void *);
  static String print_nbrs_v(Element *e, void *);
  static String print_ip(Element *e, void *);
  static String print_eth(Element *e, void *);
  static String print_links(Element *e, void *);

  static unsigned int jiff_diff_as_msec(unsigned int j1, unsigned int j2, bool &neg); // j1 - j2, in msecs
  static String jiff_diff_string(unsigned int j1, unsigned int j2);

  static String print_metric_type(Element *e, void *);
  static int write_metric_type(const String &, Element *, void *, ErrorHandler *);

  static String print_est_type(Element *e, void *);
  static int write_est_type(const String &, Element *, void *, ErrorHandler *);

  static String print_seqno(Element *e, void *);
  static int write_seqno(const String &, Element *, void *, ErrorHandler *);

  static String print_frozen(Element *e, void *);
  static int write_frozen(const String &, Element *, void *, ErrorHandler *);

  static String print_dump(Element *e, void *);

  // estimate link delivery rates, as 0-100 percent.  return false if
  // no good data available for estimation.
  bool est_forward_delivery_rate(const IPAddress &, unsigned int &);
  bool est_reverse_delivery_rate(const IPAddress &, unsigned int &);

  enum MetricType {
    MetricUnknown =                -1,
    MetricHopCount =                0, // unsigned int hop count
    MetricEstTxCount =              1, // unsigned int expected number of transmission, * 100
    MetricDeliveryRateProduct =     2, // unsigned int product of fwd delivery rates, * 100
    MetricRevDeliveryRateProduct =  3  // unsigned int product of rev delivery rates, * 100
  };

  static String metric_type_to_string(MetricType t);
  static MetricType check_metric_type(const String &);
  
  enum MetricType _metric_type;
  unsigned int    _est_type;

  // type of delivery rate estimator to use
  enum {  
    EstByMeas = 3
  };

  const metric_t _bad_metric; // default value is ``bad''

  bool _frozen;

  void dsdv_assert_(const char *, int, const char *) const;

};

inline unsigned 
dsdv_jiffies() 
{
  static unsigned last_click_jiffies = 0;
  unsigned j = click_jiffies(); 
  assert(j >= last_click_jiffies); 
  last_click_jiffies = j; 
  return j; 
}

CLICK_ENDDECLS
#endif

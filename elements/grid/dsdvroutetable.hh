#ifndef DSDVROUTETABLE_HH
#define DSDVROUTETABLE_HH

/*
 * =c
 * DSDVRouteTable(TIMEOUT, PERIOD, JITTER, ETH, IP, GridGatewayInfo, LinkTracker, LinkStat [, I<KEYWORDS>])
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
 * There must only be one of DSDVRouteTable or GridRouteTable in a
 * grid configuration.
 *
 * Regular arguments are:
 *
 * =over 8
 *
 * =item TIMEOUT
 * 
 * Unsigned integer.  Milliseconds.
 *
 * =item PERIOD
 *
 * Unsigned integer.  Milliseconds.
 *
 * =item JITTER
 *
 * Unsigned integer.  Milliseconds.
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
 * =item MAX_HOPS
 * 
 * Unsigned integer.  The maximum number of hops for which a route
 * should propagate.  The default number of hops is 3.
 *
 * =item METRIC
 *
 * String.  The type of metric that should be used to compare two
 * routes.  Allowable values are: ``hopcount'' or ``est_tx_count''
 * (estimate transmission count).  The default is to use estimated
 * transmission count.
 *
 * =item LOGFILE
 *
 * String.  Filename of file to log activity to in binary format.
 *
 * =item WST0 (zero, not``oh'')
 * 
 * Unsigned integer.  Initial weighted settling time.  Milliseconds.
 *
 * =item ALPHA
 * 
 * Double.  DSDV settling time weighting parameter.  Between 0 and 1 inclusive.
 *
 * =a
 * SendGridHello, FixSrcLoc, SetGridChecksum, LookupLocalGridRoute, LookupGeographicGridRoute
 * GridGatewayInfo, LinkStat, LinkTracker, GridRouteTable */

#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <elements/grid/gridgatewayinfo.hh>
#include <elements/grid/linktracker.hh>
#include <elements/grid/linkstat.hh>
#include "grid.hh"
#include "gridgenericrt.hh"
#include <click/timer.hh>
#include "gridlogger.hh"


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
    metric_t(unsigned int m, bool v = true) : valid(v), val(m) { }
  };
  
  /* 
   * route table entry
   */
  class RTEntry : public RouteEntry {

  public:
    class EtherAddress  dest_eth;              // hardware address of destination; 
                                               // may be all 0s if we don't hear any ads...
    bool                is_gateway;

    unsigned int        ttl;                   // msecs
    int                 last_updated_jiffies;  // last time this entry was updated

    // metrics are invalid until updated to incorporate the last hop's
    // link, i.e. by calling initialize_metric or update_metric.
    metric_t            metric;


    // DSDV book-keeping
    double              wst;                   // weighted settling time (msecs)
    metric_t            last_adv_metric;       // last metric we advertised
    int                 last_seq_jiffies;      // last time the seq_no changed
    int                 advertise_ok_jiffies;  // when it is ok to advertise route
    bool                need_seq_ad;
    bool                need_metric_ad;
    int                 last_expired_jiffies;  // when the route was expired (if broken)

    bool broken() { return num_hops == 0; }
    bool good()   { return num_hops != 0; }

    RTEntry() : 
      is_gateway(false), ttl(0), last_updated_jiffies(-1), wst(0), 
      last_seq_jiffies(0), advertise_ok_jiffies(0), need_seq_ad(false), 
      need_metric_ad(false), last_expired_jiffies(0)
    { }

    /* constructor for 1-hop route entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_hdr *gh, grid_hello *hlo,
	    unsigned int jiff) :
      RouteEntry(ip, gh->loc_good, gh->loc_err, gh->loc, eth, ip, hlo->seq_no, 1),
      dest_eth(eth), is_gateway(hlo->is_gateway), ttl(hlo->ttl), last_updated_jiffies(jiff), 
      wst(0), last_seq_jiffies(jiff), advertise_ok_jiffies(0), need_seq_ad(false), 
      need_metric_ad(false), last_expired_jiffies(0)
    { 
      loc_err = ntohs(loc_err); 
      seq_no = ntohl(seq_no); 
      ttl = ntohl(ttl);
    }

    /* constructor from grid_nbr_entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_nbr_entry *nbr, 
	    unsigned int jiff) :
      RouteEntry(nbr->ip, nbr->loc_good, nbr->loc_err, nbr->loc,
		 eth, ip, nbr->seq_no, nbr->num_hops > 0 ? nbr->num_hops + 1 : 0),
      is_gateway(nbr->is_gateway), last_updated_jiffies(jiff), 
      metric(nbr->metric, nbr->metric_valid), wst(0), last_seq_jiffies(0), 
      advertise_ok_jiffies(0), need_seq_ad(false), need_metric_ad(false),
      last_expired_jiffies(0)
    {
      loc_err = ntohs(loc_err);
      seq_no = ntohl(seq_no);
      ttl = ntohl(ttl);
      metric.val = ntohl(metric.val);
    }
    
    /* copy data from this into nb, converting to net byte order */
    void fill_in(grid_nbr_entry *nb, LinkStat *ls = 0);
    
  };
  
  typedef BigHashMap<IPAddress, RTEntry> RTable;
  typedef RTable::Iterator RTIter;
  
  /* the route table */
  // Invariants: 

  // 1. every route in the table that is not expired (num_hops > 0) is
  // valid: i.e. its ttl has not run out, nor has it been in the table
  // past its timeout.  There is a expire timer for this route in
  // _expire_timers.

  // 2. no route in the table that *is* expired (num_hops == 0) has an
  // entry in _expire_timers.

  // 3. expired routes *are* allowed in the table, since that's what
  // the DSDV description does.

  class RTable _rtes;

  void handle_update(RTEntry &, const bool was_sender);  
  void insert_route(const RTEntry &, const bool was_Sender);
  void schedule_triggered_update(const IPAddress &ip, int when); // when is in jiffies
  
  typedef BigHashMap<IPAddress, Timer *> TMap;
  typedef TMap::Iterator TMIter;

  struct HookPair {
    DSDVRouteTable *obj;
    unsigned int ip;
    HookPair(DSDVRouteTable *o, unsigned int _ip) : obj(o), ip(_ip) { }
  private:
    HookPair() { }
  };

  typedef BigHashMap<IPAddress, HookPair *> HMap;
  typedef HMap::Iterator HMIter;

  // Expire timer map invariants: every good route (r.good() is true)
  // has a running timer in _expire_timers.  No broken routes have a
  // timer.  Every entry in _expire_timers has a corresponding
  // TimerHook pointer stored in _expire_hooks.
  class TMap _expire_timers;
  class HMap _expire_hooks;

  // Trigger timer invariants: any route may have a timer in this
  // table.  A route with a timer in this table has need_seq_ad or
  // need_metric_ad set.  All timers in the table must be running.
  // Every entry in _trigger_timers has a corresponding TimerHook
  // pointer stored in _trigger_hooks.
  class TMap _trigger_timers;
  class HMap _trigger_hooks;

  /* max time to keep an entry in RT */
  unsigned int _timeout; // msecs

  /* route broadcast timing parameters */
  unsigned int _period; // msecs
  unsigned int _jitter; // msecs

  class GridGatewayInfo *_gw_info;
  class LinkTracker     *_link_tracker;
  class LinkStat        *_link_stat;

  /* binary logging */
  class GridLogger          *_log;
  static const unsigned int _log_dump_period = 5 * 1000; // msecs

  /* this node's addresses */
  IPAddress _ip;
  EtherAddress _eth;

  /* latest sequence number for this node's route entry */
  unsigned int _seq_no;
  unsigned int _fake_seq_no;
  unsigned int _bcast_count;
  unsigned int _seq_delay;

  /* local DSDV radius */
  unsigned int _max_hops;

  /* settling time constants */
  double          _alpha;
  unsigned int    _wst0;  // msecs

  /* misc. DSDV constants */
  static const unsigned int _min_triggered_update_period = 1000; // msecs

  /* track route ads */
  unsigned int _last_periodic_update;  // jiffies
  unsigned int _last_triggered_update; // jiffies

  class Timer _hello_timer;
  static void static_hello_hook(Timer *, void *e) { ((DSDVRouteTable *) e)->hello_hook(); }
  void hello_hook();

  class Timer _log_dump_timer;
  static void static_log_dump_hook(Timer *, void *e) { ((DSDVRouteTable *) e)->log_dump_hook(); }
  void log_dump_hook();


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

  static int jiff_to_msec(int j)
  { return (j * 1000) / CLICK_HZ; }

  static int msec_to_jiff(int m)
  { return (CLICK_HZ * m) / 1000; }

private:
  class RouteEntry make_generic_rte(const RTEntry &rte) { return rte; }

  /* update route metric with the last hop from the advertising node */
  void update_metric(RTEntry &);

  /* initialize the metric for a 1-hop neighbor */
  void init_metric(RTEntry &);

  /* update weight settling time */
  void update_wst(RTEntry *old_r, RTEntry &new_r);

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

  static String print_metric_type(Element *e, void *);
  static int write_metric_type(const String &, Element *, void *, ErrorHandler *);

  static String print_est_type(Element *e, void *);
  static int write_est_type(const String &, Element *, void *, ErrorHandler *);

  static String print_seq_delay(Element *e, void *);
  static int write_seq_delay(const String &, Element *, void *, ErrorHandler *);

  static int write_start_log(const String &, Element *, void *, ErrorHandler *);
  static int write_stop_log(const String &, Element *, void *, ErrorHandler *);

  static String print_frozen(Element *e, void *);
  static int write_frozen(const String &, Element *, void *, ErrorHandler *);

  bool est_forward_delivery_rate(const IPAddress &, double &);
  bool est_reverse_delivery_rate(const IPAddress &, double &);

  enum MetricType {
    MetricUnknown = -1,
    MetricHopCount = 0,            // unsigned int hop count
    MetricEstTxCount               // unsigned int expected number of transmission * 100
  };

  static String metric_type_to_string(MetricType t);
  static MetricType check_metric_type(const String &);
  
  enum MetricType _metric_type;
  unsigned int    _est_type;

  enum {  // estimator types;try to keep vals in sync with GridRouteTable
    EstByMeas = 3
  };

  static const metric_t _bad_metric; // default value is ``bad''

  bool _frozen;
};

#endif

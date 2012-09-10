#ifndef GRIDROUTETABLE_HH
#define GRIDROUTETABLE_HH

/*
 * =c
 * GridRouteTable(TIMEOUT, PERIOD, JITTER, ETH, IP, GridGatewayInfo, LinkTracker, LinkStat [, I<KEYWORDS>])
 *
 * =s Grid
 * Run DSDV-like local routing protocol
 *
 * =d
 * Implements a DSDV-like loop-free routing protocol by originating
 * routing messages based on its routing tables, and processing
 * routing update messages from other nodes.  Maintains an immediate
 * neighbor table, and a multi-hop route table.  Route entries are
 * removed TIMEOUT milliseconds after being installed.  PERIOD is the
 * milliseconds between route broadcasts, randomly offset by up to
 * JITTER milliseconds.  ETH and IP describe this node's Grid
 * addresses, and GW is the GridGatewayInfo element.  LinkTracker
 * and LinkStat are LinkTracker and LinkStat elements for the interface
 * to which this GridRouteTable element is connected.
 *
 * Routing message entries are marked with both a sequence number
 * (originated by the destination of the entry) and a real-time ttl.
 * Entries with higher sequence numbers always supersede entries with
 * lower sequence numbers.  For entries with the same sequence number,
 * the lower hop-count entry prevails.  Entry ttls decrease while the
 * entry resides in a node's routing table, as well as being decreased
 * by a minimum amount when the route is propagated to another node.
 * Thus an individual route entry will only propagate through the
 * network for a finite amount of time.  A route entry is not
 * propagated if its ttl is less than the minimum decrement amount.  A
 * route is not accepted if its ttl is <= 0.
 *
 * New routes are advertised with even sequence numbers originated by
 * the route destination (obtained from LR_HELLO messages); broken
 * routes are advertised with odd sequence numbers formed by adding 1
 * to the sequence number of the last known good route.  Broken route
 * advertisements are originally initiated when an immediate neighbor
 * entry times out, and will always supersede the route they are
 * concerned with; any new route will always supersede the previous
 * broken route.  When a node receives a broken route advertisement
 * for a destination to which it knows a newer route, it kills the
 * broken route advertisement and sends an advertisement for the the
 * new route.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item MAX_HOPS
 *
 * Integer.  The maximum number of hops for which a route should
 * propagate.  The default number of hops is 3.
 *
 * =item LOGCHANNEL
 *
 * String.  The name of the chatter channel to which route action log
 * messages should be logged.  Default channel is ``routelog''.
 *
 * =item METRIC
 *
 * String.  The type of metric that should be used to compare two
 * routes.  Allowable values are: ``hopcount'',
 * ``cumulative_delivery_rate'', ``min_delivery_rate'',
 * ``min_sig_strength'', and ``min_sig_quality''.  The default is to
 * use hopcount.
 *
 * =item LOG
 *
 * GridLogger element.  Object to log events to.
 *
 * =a
 * SendGridHello, FixSrcLoc, SetGridChecksum, LookupLocalGridRoute, UpdateGridRoutes,
 * LinkStat, LinkTracker, GridGatewayInfo, GridLogger */

#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <elements/grid/gridgatewayinfo.hh>
#include <elements/grid/linktracker.hh>
#include <elements/grid/linkstat.hh>
#include "grid.hh"
#include "gridgenericrt.hh"
#include <click/timer.hh>
CLICK_DECLS



class GridLogger;

class GridRouteTable : public GridGenericRouteTable {
public:
  // generic rt methods
  bool current_gateway(RouteEntry &entry);
  bool get_one_entry(const IPAddress &dest_ip, RouteEntry &entry);
  void get_all_entries(Vector<RouteEntry> &vec);


public:

  GridRouteTable() CLICK_COLD;
  ~GridRouteTable() CLICK_COLD;

  const char *class_name() const		{ return "GridRouteTable"; }
  void *cast(const char *);
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return "h/h"; }
  const char *flow_code() const                 { return "x/y"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  virtual bool can_live_reconfigure() const { return false; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;


private:
  /*
   * route table entry
   */
  class RTEntry {
    bool _init;

  public:
    IPAddress dest_ip; // IP address of this destination
    EtherAddress dest_eth; // Eth of this destination; may be all 0s if we don't hear any ads...
    IPAddress next_hop_ip; // IP address of next hop for this destination
    EtherAddress next_hop_eth; // hardware address of next hop

  private:
    unsigned char _num_hops; // number of hops to dest
  public:
    unsigned char num_hops() const { check(); return _num_hops; }

    void invalidate() {
      check();
      assert(_num_hops > 0 && !(_seq_no & 1));
      _num_hops = 0; _seq_no++;
      check();
    }

    struct grid_location loc; // location of dest, as contained in its route ads
    unsigned short loc_err;
    bool loc_good;
    bool is_gateway;

  private:
    unsigned int _seq_no;
  public:
    unsigned int seq_no() const { check(); return _seq_no; }
    unsigned int ttl;  // msecs

    int last_updated_jiffies; // last time this entry was updated

    unsigned int metric; // generic metric -- routing code must interpret this as neccessary
    bool metric_valid;   /* metrics are invalid until updated to
                            incorporate the last hop's link, i.e. by
                            calling initialize_metric or
                            update_metric. */

    RTEntry() :
      _init(false), _num_hops(0), loc_good(false), is_gateway(false),
      _seq_no(0), ttl(0), last_updated_jiffies(-1), metric_valid(false)
    { }

  public:
    RTEntry(IPAddress _dest_ip, IPAddress _next_hop_ip, EtherAddress _next_hop_eth,
	    unsigned char num_hops, grid_location _loc, unsigned short _loc_err,
	    bool _loc_good, bool _is_gateway, unsigned int seq_no, unsigned int _ttl,
	    unsigned int _last_updated_jiffies) :
      _init(true), dest_ip(_dest_ip), next_hop_ip(_next_hop_ip),
      next_hop_eth(_next_hop_eth), _num_hops(num_hops), loc(_loc),
      loc_err(_loc_err), loc_good(_loc_good), is_gateway(_is_gateway),
      _seq_no(seq_no), ttl(_ttl),
      last_updated_jiffies(_last_updated_jiffies), metric_valid(false)
    { check(); }

    /* constructor for 1-hop route entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_hdr *gh, grid_hello *hlo,
	    unsigned int jiff) :
      _init(true), dest_ip(ip), dest_eth(eth), next_hop_ip(ip), next_hop_eth(eth), _num_hops(1),
      loc(gh->loc), loc_good(gh->loc_good), is_gateway(hlo->is_gateway),
      last_updated_jiffies(jiff), metric_valid(false)
    {
      loc_err = ntohs(gh->loc_err);
      _seq_no = ntohl(hlo->seq_no);
      ttl = ntohl(hlo->ttl);
      check();
    }

    /* constructor from grid_nbr_entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_nbr_entry *nbr,
	    unsigned int jiff) :
      _init(true), dest_ip(nbr->ip), next_hop_ip(ip), next_hop_eth(eth),
      _num_hops(nbr->num_hops ? nbr->num_hops + 1 : 0), loc(nbr->loc), loc_good(nbr->loc_good),
      is_gateway(nbr->is_gateway), last_updated_jiffies(jiff),
      metric_valid(nbr->metric_valid)
    {
      loc_err = ntohs(nbr->loc_err);
      _seq_no = ntohl(nbr->seq_no);
      ttl = ntohl(nbr->ttl);
      metric = ntohl(nbr->metric);
      check();
    }

    /* copy data from this into nb, converting to net byte order */
    void fill_in(grid_nbr_entry *nb, LinkStat *ls = 0);
    void check() const { assert(_init); assert((_num_hops > 0) != (_seq_no & 1)); }
  };

  typedef HashMap<IPAddress, RTEntry> RTable;
  typedef RTable::iterator RTIter;

  /* the route table */
  RTable _rtes;

  void get_rtes(Vector<RTEntry> *retval);

private:
  /* max time to keep an entry in RT */
  int _timeout; // msecs, -1 if we are not timing out entries
  unsigned int _timeout_jiffies;

  /* route broadcast timing parameters */
  int _period;
  int _jitter;

  GridGatewayInfo *_gw_info;
  LinkTracker *_link_tracker;
  LinkStat *_link_stat;

  /* interval at which to check RT entries for expiry */
  static const unsigned int EXPIRE_TIMER_PERIOD = 100; // msecs

  /* extended logging */
  ErrorHandler *_extended_logging_errh;
  void log_route_table(); // print route table on 'routelog' chatter channel

  /* binary logging */
  GridLogger *_log;
  unsigned int _dump_tick;

  /* this node's addresses */
  IPAddress _ip;
  EtherAddress _eth;

  /* latest sequence number for this node's route entry */
  unsigned int _seq_no;
  unsigned int _fake_seq_no;
  unsigned int _bcast_count;
  unsigned int _seq_delay;

  /* local DSDV radius */
  int _max_hops;

  Timer _expire_timer;
  Timer _hello_timer;

  /* runs to expire route entries */
  static void expire_hook(Timer *, void *);

  /* expires routes; returns the expired routes */
  Vector<RTEntry> expire_routes();

  /* runs to broadcast route advertisements and triggered updates */
  static void hello_hook(Timer *, void *);

  /* send a route advertisement containing the entries in rte_info */
  void send_routing_update(Vector<RTEntry> &rtes_to_send, bool update_seq = true, bool check_ttls = true);

  static unsigned int decr_ttl(unsigned int ttl, unsigned int decr)
  { return (ttl > decr ? ttl - decr : 0); }

  static int jiff_to_msec(int j)
  { return (j * 1000) / CLICK_HZ; }

  static int msec_to_jiff(int m)
  { return (CLICK_HZ * m) / 1000; }

  /* update route metric with the last hop from the advertising node */
  void update_metric(RTEntry &);

  /* initialize the metric for a 1-hop neighbor */
  void init_metric(RTEntry &);

  /* true iff first route's metric is preferable to second route's
     metric -- note that this is a strict comparison, if the metrics
     are equivalent, then the function returns false.  this is
     necessary to get stability, e.g. when using the hopcount metric */
  bool metric_is_preferable(const RTEntry &, const RTEntry &);

  /* true iff we should replace the first route with the second route */
  bool should_replace_old_route(const RTEntry &, const RTEntry &);

  static String print_rtes(Element *e, void *);
  static String print_rtes_v(Element *e, void *);
  static String print_nbrs(Element *e, void *);
  static String print_nbrs_v(Element *e, void *);
  static String print_ip(Element *e, void *);
  static String print_eth(Element *e, void *);
  static String print_links(Element *e, void *);

  static String print_metric_type(Element *e, void *);
  static int write_metric_type(const String &, Element *, void *, ErrorHandler *);

  static String print_metric_range(Element *e, void *);
  static int write_metric_range(const String &, Element *, void *, ErrorHandler *);

  static String print_est_type(Element *e, void *);
  static int write_est_type(const String &, Element *, void *, ErrorHandler *);

  static String print_seq_delay(Element *e, void *);
  static int write_seq_delay(const String &, Element *, void *, ErrorHandler *);

  static String print_frozen(Element *e, void *);
  static int write_frozen(const String &, Element *, void *, ErrorHandler *);

  unsigned int qual_to_pct(int q);
  unsigned int sig_to_pct(int s);

  bool est_forward_delivery_rate(const IPAddress, double &);
  bool est_reverse_delivery_rate(const IPAddress, double &);

  enum MetricType {
    MetricUnknown = -1,
    MetricHopCount = 0,            // unsigned int hop count
    MetricCumulativeDeliveryRate,  // unsigned int percentage (0-100)
    MetricMinDeliveryRate,         // unsigned int percentage (0-100)
    MetricMinSigStrength,          // unsigned int negative dBm.  e.g. -40 dBm is 40
    MetricMinSigQuality,           // unsigned int ``quality''
    MetricCumulativeQualPct,       // unsigned int percentage (0-100) of range
    MetricCumulativeSigPct,        // unsigned int percentage (0-100) of range
    MetricEstTxCount               // unsigned int expected number of transmission * 100
  };

  static String metric_type_to_string(MetricType t);
  static MetricType check_metric_type(const String &);

  MetricType _metric_type;

  /* top and bottom of ranges for qual/sig pct */
  int _max_metric;
  int _min_metric;

  static const unsigned int _bad_metric = 7777777;

  /* default ranges taken from experiments -- from approx 144 million received packets! */
  /*
   * +-------------+-------------+-------------+-------------+--------------+--------------+--------------+--------------+
   * | min(signal) | max(signal) | std(signal) | avg(signal) | min(quality) | max(quality) | std(quality) | avg(quality) |
   * +-------------+-------------+-------------+-------------+--------------+--------------+--------------+--------------+
   * |        -100 |         -13 |     13.0719 |    -69.8756 |            0 |          130 |       3.6859 |       6.7074 |
   * +-------------+-------------+-------------+-------------+--------------+--------------+--------------+--------------+
   */
  static const int _max_qual = 130;
  static const int _min_qual = 0;
  static const int _max_sig = -13;
  static const int _min_sig = -100;


  enum {
    EstByQual = 0,
    EstBySig,
    EstBySigQual,
    EstByMeas
  };

  unsigned int _est_type;

  bool _frozen;

  RouteEntry make_generic_rte(const RTEntry &rte) {
    return RouteEntry(rte.dest_ip, rte.loc_good, rte.loc_err, rte.loc,
		      rte.next_hop_eth, rte.next_hop_ip,
		      0, // ignore interface number information
		      rte.seq_no(), rte.num_hops());
  }

};

CLICK_ENDDECLS
#endif

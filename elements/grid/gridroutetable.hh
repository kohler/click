#ifndef GRIDROUTETABLE_HH
#define GRIDROUTETABLE_HH

/*
 * =c
 * GridRouteTable(TIMEOUT, PERIOD, JITTER, ETH, IP, GW [, I<KEYWORDS>])
 *
 * =s Grid
 * Run DSDV-like local routing protocol
 *
 * =d Implements a DSDV-like loop-free routing protocol by originating
 * routing messages based on its routing tables, and processing
 * routing update messages from other nodes.  Maintains an immediate
 * neighbor table, and a multi-hop route table.  Route entries are
 * removed TIMEOUT milliseconds after being installed.  PERIOD is the
 * milliseconds between route broadcasts, randomly offset by up to
 * JITTER milliseconds.  ETH and IP describe this node's Grid
 * addresses, and GW is the GridGatewayInfo element.
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
 * ``cumulative_delivery_rate'', ``min_delivery_rate'', and
 * ``min_sig_strength''.  The default is to use hopcount.
 *
 * =a
 * SendGridHello, FixSrcLoc, SetGridChecksum, LookupLocalGridRoute, UpdateGridRoutes */

#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <elements/grid/gridgatewayinfo.hh>
#include "grid.hh"
#include <click/timer.hh>

class GridRouteTable : public Element {

public:

  GridRouteTable();
  ~GridRouteTable();

  const char *class_name() const		{ return "GridRouteTable"; }
  void *cast(const char *);
  const char *processing() const		{ return "h/h"; }
  const char *flow_code() const                 { return "x/y"; }
  GridRouteTable *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  virtual bool can_live_reconfigure() const { return false; }

  Packet *simple_action(Packet *);

  void add_handlers();
  
  /* 
   * route table entry
   */
  class RTEntry {
    bool _init;

  public:
    IPAddress dest_ip; // IP address of this destination
    IPAddress next_hop_ip; // IP address of next hop for this destination
    EtherAddress next_hop_eth; // hardware address of next hop
    unsigned char num_hops; // number of hops to dest

    struct grid_location loc; // location of dest, as contained in its route ads
    unsigned short loc_err;
    bool loc_good;
    bool is_gateway;

    unsigned int seq_no; 
    unsigned int ttl;  // msecs

    int last_updated_jiffies; // last time this entry was updated

    unsigned int metric; // generic metric -- routing code must interpret this as neccessary
    bool metric_valid;   /* metrics are invalid until updated to
                            incorporate the last hop's link, i.e. by
                            calling initialize_metric or
                            update_metric. */


    RTEntry() : 
      _init(false), num_hops(0), loc_good(false), is_gateway(false), 
      seq_no(0), ttl(0), last_updated_jiffies(-1), metric_valid(false) { }
    
    RTEntry(IPAddress _dest_ip, IPAddress _next_hop_ip, EtherAddress _next_hop_eth,
	    unsigned char _num_hops, grid_location _loc, unsigned short _loc_err, 
	    bool _loc_good, bool _is_gateway, unsigned int _seq_no, unsigned int _ttl, 
	    int _last_updated_jiffies) :
      _init(true), dest_ip(_dest_ip), next_hop_ip(_next_hop_ip), 
      next_hop_eth(_next_hop_eth), num_hops(_num_hops), loc(_loc), 
      loc_err(_loc_err), loc_good(_loc_good), is_gateway(_is_gateway), 
      seq_no(_seq_no), ttl(_ttl),
      last_updated_jiffies(_last_updated_jiffies), metric_valid(false)
    { }

    /* constructor for 1-hop route entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_hdr *gh, grid_hello *hlo,
	    unsigned int jiff) :
      _init(true), dest_ip(ip), next_hop_ip(ip), next_hop_eth(eth), num_hops(1), 
      loc(gh->loc), loc_good(gh->loc_good), is_gateway(hlo->is_gateway),
      last_updated_jiffies(jiff), metric_valid(false)
    { 
      loc_err = ntohs(gh->loc_err); 
      seq_no = ntohl(hlo->seq_no); 
      ttl = ntohl(hlo->ttl);
    }

    /* constructor from grid_nbr_entry, converting from net byte order */
    RTEntry(IPAddress ip, EtherAddress eth, grid_nbr_entry *nbr, 
	    unsigned int jiff) :
      _init(true), dest_ip(nbr->ip), next_hop_ip(ip), next_hop_eth(eth),
      num_hops(nbr->num_hops + 1), loc(nbr->loc), loc_good(nbr->loc_good),  
      is_gateway(nbr->is_gateway), last_updated_jiffies(jiff), 
      metric(nbr->metric), metric_valid(false)
    {
      loc_err = ntohs(nbr->loc_err);
      seq_no = ntohl(nbr->seq_no);
      ttl = ntohl(nbr->ttl);
    }
    
    /* copy data from this into nb, converting to net byte order */
    void fill_in(grid_nbr_entry *nb);
    
  };
  friend class RTEntry;
  
  typedef BigHashMap<IPAddress, RTEntry> RTable;
  typedef RTable::Iterator RTIter;

  /* the route table */
  RTable _rtes;

  void get_rtes(Vector<RTEntry> *retval);

  const RTEntry *current_gateway();
  
private:
  /* max time to keep an entry in RT */
  int _timeout; // msecs, -1 if we are not timing out entries
  int _timeout_jiffies;

  /* route broadcast timing parameters */
  int _period;
  int _jitter;

  GridGatewayInfo *_gw_info;

  /* interval at which to check RT entries for expiry */
  static const unsigned int EXPIRE_TIMER_PERIOD = 100; // msecs

  /* extended logging */
  ErrorHandler *_extended_logging_errh;
  void GridRouteTable::log_route_table(); // print route table on 'routelog' chatter channel

  /* this node's addresses */
  IPAddress _ip;
  EtherAddress _eth;

  /* latest sequence number for this node's route entry */
  unsigned int _seq_no;

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

  /* initialize metric for one-hop route */
  void initialize_metric(RTEntry &);

  /* true iff first route's metric is preferable to second route's metric */
  bool metric_preferable(const RTEntry &, const RTEntry &);

  static String print_rtes(Element *e, void *);
  static String print_rtes_v(Element *e, void *);
  static String print_nbrs(Element *e, void *);
  static String print_ip(Element *e, void *);
  static String print_eth(Element *e, void *);
  static String print_metric_type(Element *e, void *);

  static int write_metric_type(const String &, Element *, void *, ErrorHandler *);

  static int check_metric_type(const String &);
  

  enum MetricType {
    MetricHopCount = 0,            // unsigned int hop count
    MetricCumulativeDeliveryRate,  // unsigned int percentage (0-100)
    MetricMinDeliveryRate,         // unsigned int percentage (0-100)
    MetricMinSigStrength           // unsigned int negative dBm.  e.g. -40 dBm is 40
  };

  int _metric_type;
    

};

#endif

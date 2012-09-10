#ifndef UPDATEROUTE_HH
#define UPDATEROUTE_HH

/*
 * =c
 * UpdateGridRoutes(TIMEOUT, PERIOD, JITTER, ETH, IP [, MAXHOPS])
 *
 * =s Grid
 * Run DSDV-like local routing protocol.
 *
 * =deprecated GridRouteTable
 *
 * =d
 *
 * Implements a DSDV-like loop-free routing protocol by originating
 * routing messages based on its routing and neighbor tables, and
 * processing routing update messages from other nodes.  Maintains an
 * immediate neighbor table, and a multi-hop route table.  For both
 * tables, entries are removed TIMEOUT milliseconds after being
 * installed.
 *
 * Routing message entries are marked with both a sequence number
 * (originated by the destination of the entry) and an age.  Entries
 * with higher sequence numbers always supersede entries with lower
 * sequence numbers.  For entries with the same sequence number, the lower
 * hop-count entry prevails.  Entries increase in age while sitting in
 * a node's routing table, as well as when being propagated to another
 * node.  Thus an individual entry will only propagate through the
 * network for a finite amount of time.
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
 * Input 0 expects MAC layer Grid packets which are handled as
 * necessary and sent to output 0.
 *
 * Output 1: every PERIOD milliseconds (+/- a jitter bounded by JITTER
 * milliseconds), emits a Grid protocol ``LR_HELLO'' packet for the
 * Grid node at address IP with MAC address ETH, advertising any
 * neighbors within MAXHOPS of the node.  MAXHOPS defaults to 3.
 * PERIOD must be greater than 0, JITTER must be positive and less
 * than JITTER.  Produces MAC layer Grid packets.
 *
 * This element is deprecated.  Use GridRouteTable instead.
 *
 * =a
 * SendGridHello, FixSrcLoc, SetGridChecksum, LookupLocalGridRoute, GridRouteTable */


#include <click/element.hh>
#include <click/glue.hh>
#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include "grid.hh"
#include <click/timer.hh>
CLICK_DECLS

class UpdateGridRoutes : public Element {

public:

/* NOTE ON TIMEOUTS: there are two different timeouts.  The `age' of a
   routing entry is approximately how much time has passed since the
   entry was originally produced by the node it points too.  The age
   is increased everytime an entry is propagated, as well as with the
   passage of time, to prevent stale data from sticking around
   (actually, the age is set, and then decremented, the entry is
   thrown away when it's age is ~0).

   The ``jiffies' of an entry are how long that entry has been in this
   particular routing table, we discard entries that are too old here
   also.  It could be that the age makes jiffies redundant.

   of course, we are comforted with this: jiffies are a local
   decision, while age is decided by the initiator of the entry */


  class NbrEntry {
    bool _init;
  public:
    EtherAddress eth;
    IPAddress ip;
    unsigned int last_updated_jiffies;
    NbrEntry() : _init(false), last_updated_jiffies(0) { }
    NbrEntry(EtherAddress eth_in, IPAddress ip_in, unsigned int jiff)
      : _init(true), eth(eth_in), ip(ip_in), last_updated_jiffies(jiff) { }
    operator bool() const { return _init; }
    unsigned int hashcode() const { return *(unsigned int *)ip.data(); }
    String s() const
    { return eth.unparse() + " -- " + ip.unparse() + " -- " + String(last_updated_jiffies); }
  };

  typedef HashMap<IPAddress, NbrEntry> Table;
  Table _addresses; // immediate nbrs
  /*
   * _addresses is a mapping from IP to ether for nodes within our
   * radio range.  this information is extracted by snooping on all
   * packets with grid headers .
   */

  struct far_entry {
    far_entry() : last_updated_jiffies(0) { }
    far_entry(int j, grid_nbr_entry n) : last_updated_jiffies(j), sent_new(false), nbr(n)  { }
    unsigned int last_updated_jiffies;
    bool sent_new;
    grid_nbr_entry nbr;
  };

  typedef HashMap<IPAddress, far_entry> FarTable;
  FarTable _rtes; // immediate and multihop nbrs
  /*
   * _rtes is our routing table; its information is maintained by
   * processing Grid Hello (GRID_LR_HELLO) packets only.  some
   * invariants: any entry listed as one hop in _rtes has an entry in
   * _addresses (but not neccesarily vice-versa right now...).  this
   * table should never include a broken route (indicated by num_hops
   * == 0).  There may be entries with age 0 */

  /*
   * will leave _addresses and _rtes exposed to other code so we can
   * eliminate implementing all the routing lookup logic in this
   * class.  This class will only worry about keeping the tables
   * updated.  This assumes single thread -- e.g. when we are looking
   * up in the tables from other code, we will neve be in the middle
   * of updating the table.  Also, the tables are public so the static
   * read handlers can access them! -- perhaps eventually need a
   * better design for accessing the table contents...
   */


  UpdateGridRoutes() CLICK_COLD;
  ~UpdateGridRoutes() CLICK_COLD;

  const char *class_name() const		{ return "UpdateGridRoutes"; }
  void *cast(const char *);
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

  void get_rtes(Vector<grid_nbr_entry> *retval);

  IPAddress _ipaddr;
  EtherAddress _ethaddr;

private:
  int _timeout; // -1 if we are not timing out entries
  unsigned int _timeout_jiffies;

  int _max_hops;

  int _period;
  int _jitter;
  Timer _hello_timer;

  static const unsigned int EXPIRE_TIMER_PERIOD = 100; // msecs
  Timer _expire_timer;

  /* check we aren't going crazy with the routing updates... */
  static const int SANITY_CHECK_PERIOD = 10000; // 10 secs
  static const int SANITY_CHECK_MAX_PACKETS = 100;
  Timer _sanity_timer;
  int _num_updates_sent;

  unsigned int _seq_no;

  static void expire_hook(Timer *, void *);
  static void hello_hook(Timer *, void *);
  static void sanity_hook(Timer *, void *);

  Vector<grid_nbr_entry> expire_routes();
  void send_routing_update(Vector<grid_nbr_entry> &rte_info, bool);
#if 0
  Packet *make_hello();
#endif
  // decrement, bottoming out at 0
  static unsigned int decr_age(unsigned int age, unsigned int decr)
  { return (age > decr ? age - decr : 0); }
};

CLICK_ENDDECLS
#endif

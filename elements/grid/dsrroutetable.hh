#ifndef DSRROUTETABLE_HH
#define DSRROUTETABLE_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>
#include <click/hashmap.hh>
#include <assert.h>
#include <click/bighashmap.hh>

#include <elements/wifi/linktable.hh>
#include <elements/standard/simplequeue.hh>

#include "dsr.hh"

CLICK_DECLS
class LinkTable;


/*
 * =c
 * DSRRouteTable(IP, LINKTABLE, [, I<KEYWORDS>])
 *
 * =s Grid
 *
 * A DSR protocol implementation
 *
 * =d
 *
 * This is meant to approximate an `official' implementation of DSR as of version
 * 10 of the IETF draft.  Network-layer acknowledgements and many optimizations (e.g.
 * reply-from-cache and route shortening) are not implemented.
 *
 * Regular arguments are:
 *
 * =over 8
 * =item IP
 *
 * This node's IP address.
 *
 * =item LINKTABLE
 *
 * A LinkTable element which will function as a link cache for the protocol.
 *
 * =back
 *
 * Keywords arguments are:
 *
 * =over 8
 * =item OUTQUEUE
 *
 * A SimpleQueue from which the DSRRouteTable element will "yank" packets in the
 * event of a transmission error or received route error message.
 *
 * =item METRIC
 *
 * A GridGenericMetric element to use for obtaining link metrics.  If
 * not specified, minimum hop-count is used.
 *
 * =item USE_BLACKLIST
 *
 * Boolean.  Whether or not to perform "blacklisting" of links that appear to be
 * unidirectional.  See Section 4.6 of the IETF draft.  Default is true.
 *
 * =back
 * =a
 * DSRArpTable */

/*
   todo:
   - combine routetable and arptable and split things up more sensibly, into
     more managable elements
   - fill out the rest of the spec: network layer acks, optimizations
   - proper handling of transmission errors on RERR messages
   - proper handling of packets going over one hop with no SR option
   - eliminate metric field when not using ETX; new option type?
*/

class GridGenericMetric;

class DSRRouteTable : public Element
{
public:

  // IP packets buffered while waiting for route replies
  class BufferedPacket
  {
  public:
    Packet *_p;
    Timestamp _time_added;

    BufferedPacket(Packet *p) {
      assert(p);
      _p=p;
      _time_added.assign_now();
    }
    void check() const { assert(_p); }
  };

#define DSR_SENDBUFFER_MAX_LENGTH     5     // maximum number of packets to buffer per destination
#define DSR_SENDBUFFER_MAX_BURST      5     // maximum number of packets to send in one sendbuffer check
#define DSR_SENDBUFFER_TIMEOUT        5000 // how long packets can live in the sendbuffer (ms)
#define DSR_SENDBUFFER_TIMER_INTERVAL 1000  // how often to check for expired packets (ms)

  typedef Vector<BufferedPacket> SendBuffer;
  typedef HashMap<IPAddress, SendBuffer> SBMap;
  typedef SBMap::iterator SBMapIter;

  // info about route requests we're forwarding (if waiting for a
  // unidirectionality test result) and those we've forwarded lately.
  // kept in a hash table indexed by (src,target,id).  these entries
  // expire after some time.
  class ForwardedReqKey
  {
  public:
    IPAddress _src;
    IPAddress _target;
    unsigned int _id;

    ForwardedReqKey(IPAddress src, IPAddress target, unsigned int id) {
      _src = src; _target = target; _id = id;
      check();
    }
    ForwardedReqKey() {
      // need this for bighashmap::pair to work
    }
    bool operator==(const ForwardedReqKey &f1) {
      check();
      f1.check();
      return ((_src    == f1._src) &&
	      (_target == f1._target) &&
	      (_id     == f1._id));
    }
    inline size_t hashcode() const;
    void check() const {
      assert(_src);
      assert(_target);
      assert(_src != _target);
    }
  };

  class ForwardedReqVal {
  public:
    Timestamp _time_forwarded;

    unsigned short best_metric; // best metric we've forwarded so far

    // the following two variables are set if we're waiting for a
    // unidirectionality test (RREQ with TTL 1) to come back
    Timestamp _time_unidtest_issued;
    Packet *p;

    void check() const {
      assert(_time_forwarded > 0);
      assert(best_metric > 0);
    }
  };

#define DSR_RREQ_TIMEOUT 600000 // how long before we timeout entries (ms)
#define DSR_RREQ_EXPIRE_TIMER_INTERVAL 15000 // how often to check (ms)

  typedef HashMap<ForwardedReqKey, ForwardedReqVal> ForwardedReqMap;
  typedef ForwardedReqMap::iterator FWReqIter;


  // blacklist of unidirectional links
  //
  // - when you receive a route reply, remove the link from the list
  // - when you forward a source-routed packet, remove the link from the list?
  // - when you forward a route request, drop it if it's unidirectionality is 'probable'
  // - if it's 'questionable', issue a RREQ with ttl 1
  // - entries go from probable -> questionable after some amount of time

#define DSR_BLACKLIST_NOENTRY            1
#define DSR_BLACKLIST_UNI_PROBABLE       2
#define DSR_BLACKLIST_UNI_QUESTIONABLE   3
#define DSR_BLACKLIST_UNITEST_TIMEOUT    1000 // ms
#define DSR_BLACKLIST_TIMER_INTERVAL     300 // how often to check for expired entries (ms)
#define DSR_BLACKLIST_ENTRY_TIMEOUT      45000 // how long until entries go from 'probable' to 'questionable'

  class BlacklistEntry {
  public:
    Timestamp _time_updated;
    int _status;

    void check() const {
      assert(_time_updated > 0);
      switch (_status) {
      case DSR_BLACKLIST_NOENTRY:
      case DSR_BLACKLIST_UNI_PROBABLE:
      case DSR_BLACKLIST_UNI_QUESTIONABLE:
	break;
      default:
	assert(0);
      }
    }
  };
  typedef HashMap<IPAddress, BlacklistEntry> Blacklist;
  typedef Blacklist::iterator BlacklistIter;


  // info about the last request we've originated to each target node.
  // these are kept in a hash table indexed by the target IP.  when a
  // route reply is received from a host, we remove the entry.

  class InitiatedReq
  {
  public:

    IPAddress _target;
    int _ttl; // ttl used on the last request
    Timestamp _time_last_issued;

    // number of times we've issued a request to this target since
    // last receiving a reply
    unsigned int _times_issued;

    // time from _time_last_issued until we can issue another, in ms
    unsigned long _backoff_interval;

  // first request to a new target has TTL1.  host waits
  // INITIAL_DELAY, if no response then it issues a new request with
  // TTL2, waits DELAY2.  subsequent requests have TTL2, but are
  // issued after multiplying the delay by the backoff factor.
#define DSR_RREQ_TTL1            255  // turn this off for the etx stuff
#define DSR_RREQ_TTL2            255
#define DSR_RREQ_DELAY1          500  // ms
#define DSR_RREQ_DELAY2          500  // ms
#define DSR_RREQ_BACKOFF_FACTOR  2
#define DSR_RREQ_MAX_DELAY       5000 // uh, reasonable?

#define DSR_RREQ_ISSUE_TIMER_INTERVAL 300 // how often to check if its time to issue a new request (ms)

    InitiatedReq(IPAddress targ) {
      _target = targ;
      _ttl = DSR_RREQ_TTL1;
      _times_issued = 1;
      _backoff_interval = DSR_RREQ_DELAY1;

      _time_last_issued.assign_now();

      check();
    }
    InitiatedReq() {
      // need this for bighashmap::pair to work
    }
    void check() const {
      assert(_target);
      assert(_ttl > 0);
      assert(_time_last_issued > 0);
      assert(_times_issued > 0);
      assert(_backoff_interval > 0);
    }
  };

  typedef HashMap<IPAddress, InitiatedReq> InitiatedReqMap;
  typedef InitiatedReqMap::iterator InitReqIter;
  typedef HashMap<IPAddress, Packet *> RequestsToForward;

public:

  DSRRouteTable() CLICK_COLD;
  ~DSRRouteTable() CLICK_COLD;

  const char *class_name() const { return "DSRRouteTable"; }
  const char *port_count() const { return "3/3"; }
  const char *processing() const { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  int initialize(ErrorHandler *) CLICK_COLD;
  void uninitialize();

  void push(int, Packet *);

  static void static_rreq_expire_hook(Timer *, void *);
  void rreq_expire_hook();

  static void static_rreq_issue_hook(Timer *, void *);
  void rreq_issue_hook();

  static void static_sendbuffer_timer_hook(Timer *, void *);
  void sendbuffer_timer_hook();

  static void static_blacklist_timer_hook(Timer *, void *);
  void blacklist_timer_hook();

  void check();

  static DSRRoute extract_request_route(const Packet *);
  static DSRRoute extract_reply_route(const Packet *);
  static DSRRoute extract_source_route(const Packet *, unsigned int);

  static DSRRoute reverse_route(DSRRoute route);
  static int route_index_of(DSRRoute r, IPAddress ip);
  static DSRRoute truncate_route(DSRRoute r, IPAddress ip);

  struct link_filter {
    const IPAddress _a;
    const IPAddress _b;
    link_filter(IPAddress a, IPAddress b) : _a(a), _b(b) { }
    bool operator()(const Packet *p) {
      // returns true for packets which have a source route which contains
      // the link specified (_a,_b)

      // click_chatter("link_filter: checking packet %ld\n", (long)p);

      assert(p->has_network_header());
      const click_ip *ip = p->ip_header();

      if (ip->ip_p != IP_PROTO_DSR)
	return false;

      const click_dsr *dsr = (const click_dsr *)(p->data() + sizeof(click_ip));
      unsigned int dsr_len = ntohs(dsr->dsr_len);

      char *end = (char *)((char *)dsr + dsr_len);

      const click_dsr_option *op = (const click_dsr_option *)((char *)dsr + sizeof(click_dsr));

      while ((char *)op < end) {
	// DEBUG_CHATTER("option at %ld < %ld\n", (long)op, (long)end);

	if (op->dsr_type == DSR_TYPE_RREP) {
	  const click_dsr_rrep *rrep = (const click_dsr_rrep *)op;
	  op = (const click_dsr_option *)(rrep->next_option());
	} else if (op->dsr_type == DSR_TYPE_RREQ) {
	  const click_dsr_rreq *rreq = (const click_dsr_rreq *)op;
	  op = (const click_dsr_option *)(rreq->next_option());
	} else if (op->dsr_type == DSR_TYPE_RERR) {
	  const click_dsr_rerr *rerr = (const click_dsr_rerr *)op;
	  op = (const click_dsr_option *)(rerr->next_option());
	} else if (op->dsr_type == DSR_TYPE_SOURCE_ROUTE) {
	  int offset = (unsigned char *)op - p->data();
	  DSRRoute r = extract_source_route(p, offset);

	  click_chatter("link_filter: packet has source route:\n");
	  for (int i=0; i<r.size(); i++)
	    click_chatter(" - %d  %s (%d)\n",
			  i,
			  r[i].ip().unparse().c_str(),
			  r[i]._metric);

	  int i1 = route_index_of(r, _a);
	  if (i1 == -1) return false;

	  int i2 = route_index_of(r, _b);
	  if (i2 == -1) return false;

	  click_chatter("link_filter: src/dst is %s/%s (%d/%d)\n",
			_a.unparse().c_str(), _b.unparse().c_str(), i1, i2);

	  /* XXX we're already assuming bidirectionality, so this abs
	   * seems ok; really we should probably be checking the order,
	   * and we should check if the packet has already been forwarded
	   * past this hop, but this will do for now */
	  if (abs(i1 - i2) == 1) {
	    click_chatter("link_filter: true\n");
	    return true;
	  } else {
	    click_chatter("link_filter: false\n");
	    return false;
	  }
	}
      }

      // couldn't find source route option
      return false;
    }
  };


private:

  IPAddress *me;

  LinkTable *_link_table;

  SBMap _sendbuffer_map;
  ForwardedReqMap _forwarded_rreq_map;
  InitiatedReqMap _initiated_rreq_map;

  Blacklist _blacklist;
  RequestsToForward _rreqs_to_foward;

  Timer _rreq_expire_timer;
  Timer _rreq_issue_timer;
  Timer _sendbuffer_timer;
  bool _sendbuffer_check_routes;
  Timer _blacklist_timer;

  u_int16_t _rreq_id;

  SimpleQueue *_outq;
  GridGenericMetric *_metric;
  bool _use_blacklist;

  Packet *add_dsr_header(Packet *, const Vector<IPAddress> &);
  Packet *strip_headers(Packet *);

  void start_issuing_request(IPAddress host);
  void stop_issuing_request(IPAddress host);

  void issue_rreq(IPAddress dst, unsigned int ttl, bool unicast);
  void issue_rrep(IPAddress src, IPAddress dst,
		  DSRRoute reply_route,
		  DSRRoute source_route);

  void issue_rerr(IPAddress bad_src, IPAddress bad_dst,
		  IPAddress src,
		  DSRRoute source_route);

  void forward_rreq(Packet *);
  void forward_rrep(Packet *);
  void forward_rerr(Packet *);
  void forward_data(Packet *);
  static IPAddress next_sr_hop(Packet *, unsigned int);
  void forward_sr(Packet *, unsigned int, int);

  void add_route_to_link_table(DSRRoute route);

  void buffer_packet(Packet *p);

  int check_blacklist(IPAddress ip);
  void set_blacklist(IPAddress ip, int s);

  static unsigned long diff_in_ms(const Timestamp &, const Timestamp &);

  static IPAddress next_hop(Packet *p);

  unsigned char get_metric(EtherAddress);
  unsigned short route_metric(DSRRoute r);
  bool metric_preferable(unsigned short a, unsigned short b);

  void flush_sendbuffer();

  EtherAddress last_forwarder_eth(Packet *);
  bool _debug;
};

inline size_t DSRRouteTable::ForwardedReqKey::hashcode() const {
  return ((unsigned int)( // XXX is this reasonable?
			 ((_src.addr() << 16) + (_src.addr() >> 16)) ^
			 _target.addr() ^
			 _id));
}

CLICK_ENDDECLS
#endif

#ifndef LINKSTATHH
#define LINKSTATHH

/*
 * =c
 * LinkStat([I<KEYWORDS>])
 * =s Grid
 * Track broadcast loss rates. 
 *
 * =d
 *
 * Expects Grid link probe packets as input.  Records the last WINDOW unique
 * (not neccessarily sequential) sequence numbers of link probes from
 * each host, and calculates loss rates over the last TAU milliseconds
 * for each host.  If the output is connected, sends probe
 * packets every PERIOD milliseconds.  The source Ethernet and IP
 * addresses (ETH and IP) must be specified if the second output is
 * connected.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ETH, IP
 *
 * Ethernet and IP addresses of this node, respectively; required if
 * output is connected.
 *
 * =item PERIOD
 *
 * Unsigned integer.  Millisecond period between sending link probes
 * if second output is connected.  Defaults to 1000 (1 second).
 *
 * =item WINDOW
 *
 * Unsigned integer.  Number of most recent sequence numbers to remember
 * for each host.  Defaults to 100.
 *
 * =item TAU
 *
 * Unsigned integer.  Millisecond period over which to calculate loss
 * rate for each host.  Defaults to 10,000 (10 seconds).
 *
 * =item SIZE
 *
 * Unsigned integer.  Total number of bytes in probe packet.  Defaults to 1000.
 *
 *
 * =back
 *
 * =a
 * LinkTracker, PingPong */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <elements/grid/timeutils.hh>
#include <elements/grid/grid.hh>

CLICK_DECLS

class Timer;
class grid_link_probe;

class LinkStat : public Element {
private:
  unsigned int _window; // sequence numbers
  unsigned int _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _probe_size; // bytes

  unsigned int _seq;
  IPAddress    _ip;
  EtherAddress _eth;


  // record probes received from other hosts
  struct probe_t {
    struct timeval when;  
    unsigned int   seq_no;  
    probe_t(const struct timeval &t, unsigned int s) : when(t), seq_no(s) { }
  };

  struct probe_list_t {
    unsigned int    ip;
    unsigned int    period;   // period of this node's probes, as reported by the node
    unsigned int    tau;      // this node's stats averaging period, as reported by the node
    Vector<probe_t> probes;   // most recently received probes
    probe_list_t(unsigned int ip_, unsigned int p, unsigned int t) : ip(ip_), period(p), tau(t) { }
    probe_list_t() : ip(0), period(0), tau(0) { }
  };

  // Per-sender map of received probes.
  typedef BigHashMap<IPAddress, probe_list_t> ProbeMap;
  ProbeMap _bcast_stats;

  // record delivery date data about our outgoing links
  struct outgoing_link_entry_t : public grid_link_entry {
    struct timeval received_at;
    unsigned int   tau;
    outgoing_link_entry_t() { memset(this, 0, sizeof(*this)); }
    outgoing_link_entry_t(grid_link_entry *l, const struct timeval &now, unsigned int t) 
      : received_at(now), tau(t) {
#ifndef SMALL_GRID_PROBES
      ip = l->ip;
      period = ntohl(l->period);
      num_rx = ntohl(l->num_rx);
      last_rx_time = ntoh(l->last_rx_time);
      last_seq_no = ntohl(l->last_seq_no);
#else
      ip = l->ip & 0xff;
      num_rx = l->num_rx;
#endif
    }
  };

  // Per-receiver map of delivery rate data
  typedef BigHashMap<IPAddress, outgoing_link_entry_t> ReverseProbeMap;
  ReverseProbeMap _rev_bcast_stats;

  static String read_stats(Element *, void *);
  static String read_bcast_stats(Element *, void *);

  // count number of probes received from specified host during last
  // _tau msecs.
  unsigned int count_rx(const EtherAddress &);
  unsigned int count_rx(const IPAddress &);
  unsigned int count_rx(const probe_list_t *);
  
  // handlers
  static String read_window(Element *, void *);
  static String read_period(Element *, void *);
  static String read_tau(Element *, void *);
  static int write_window(const String &, Element *, void *, ErrorHandler *);
  static int write_period(const String &, Element *, void *, ErrorHandler *);
  static int write_tau(const String &, Element *, void *, ErrorHandler *);
  
  void add_bcast_stat(const IPAddress &, const grid_link_probe *);

  static void static_send_hook(Timer *, void *e) { ((LinkStat *) e)->send_hook(); }
  void send_hook();

  Timer *_send_timer;
  struct timeval _next_bcast;

  static unsigned int calc_pct(unsigned tau, unsigned period, unsigned num_rx);

 public:
  // Get forward delivery rate R to host IP over period TAU
  // milliseconds, as recorded at time T.  R is a percentage (0-100).
  // Return true iff we have data.
  bool get_forward_rate(const IPAddress &ip, unsigned int *r, unsigned int *tau, 
			struct timeval *t);

  // Get reverse delivery rate R from host IP over period TAU
  // milliseconds, as of now.  R is a percentage 0-100.  Return true
  // iff we have good data.
  bool get_reverse_rate(const IPAddress &ip, unsigned int *r, unsigned int *tau);

  LinkStat();
  ~LinkStat();
  
  const char *class_name() const		{ return "LinkStat"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const                 { return "x/y"; }
  void notify_noutputs(int);
  
  LinkStat *clone() const;

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif

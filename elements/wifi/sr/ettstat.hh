#ifndef ETTSTATHH
#define ETTSTATHH

/*
 * =c
 * ETTStat([I<KEYWORDS>])
 * =s Grid
 * Track broadcast loss rates. 
 *
 * =d
 *
 * Expects Link probe packets as input.  Records the last WINDOW unique
 * (not neccessarily sequential) sequence numbers of link probes from
 * each host, and calculates loss rates over the last TAU milliseconds
 * for each host.  If the output is connected, sends probe
 * packets every PERIOD milliseconds.  The source Ethernet 
 * address ETH must be specified if the second output is
 * connected.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ETH
 *
 * Ethernet address of this node; required if output is connected.
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
 * Unsigned integer.  Total number of bytes in probe packet, including
 * ethernet header and above.  Defaults to 1000.
 *
 *
 * =back
 */

#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <elements/wifi/arptable.hh>
CLICK_DECLS

class Timer;

class ETTStat : public Element {

public:

  // Packet formats:

  // ETTStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

  enum {
    PROBE_SMALL = (1<<0),
    PROBE_2 = (1<<1),
    PROBE_4 = (1<<2),
    PROBE_11 = (1<<5),
    PROBE_22 = (1<<11),
  };
  struct link_probe {
    unsigned short cksum;     // internet checksum
    unsigned short psz;       // total packet size, including eth hdr

    uint32_t ip;
    uint32_t flags;
    unsigned int seq_no;
    unsigned int period;      // period of this node's probe broadcasts, in msecs
    unsigned int tau;         // this node's loss-rate averaging period, in msecs
    unsigned int sent;        // how many probes this node has sent
    unsigned int num_links;   // number of wifi_link_entry entries following

    link_probe() : cksum(0), psz(0), ip(0), seq_no(0), period(0), tau(0), sent(0), num_links(0) { }

  };
  
  struct link_entry {
    uint8_t fwd_small;
    uint8_t rev_small;
    uint8_t fwd_2;
    uint8_t rev_2;
    uint8_t fwd_4;
    uint8_t rev_4;
    uint8_t fwd_11;
    uint8_t rev_11;
    uint8_t fwd_22;
    uint8_t rev_22;

    uint32_t ip;
    link_entry() { }
    link_entry(IPAddress __ip) : ip(__ip.addr()) { }
  };

private:
  unsigned int _window; // sequence numbers
  unsigned int _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _probe_size; // bytes

  unsigned int _seq;
  uint32_t _sent;
  EtherAddress _eth;
public:
  IPAddress _ip;
  IPAddress reverse_arp(EtherAddress);
  void take_state(Element *, ErrorHandler *);
  Vector<IPAddress> get_neighbors() {
    return _neighbors;
  }
private:
  class ETTMetric *_ett_metric;
  class ARPTable *_arp_table;
  uint16_t _et;     // This protocol's ethertype

  struct timeval _start;
  // record probes received from other hosts
  struct probe_t {
    struct timeval when;  
    unsigned int   seq_no;  
    probe_t(const struct timeval &t, unsigned int s) : when(t), seq_no(s) { }
  };

  struct probe_list_t {
    IPAddress ip;
    int period;   // period of this node's probes, as reported by the node
    int tau;      // this node's stats averaging period, as reported by the node
    int sent;

    uint8_t fwd_small;
    uint8_t fwd_2;
    uint8_t fwd_4;
    uint8_t fwd_11;
    uint8_t fwd_22;

    struct timeval last_rx;
    DEQueue<probe_t> probes_small;   // most recently received probes
    DEQueue<probe_t> probes_2;   // most recently received probes
    DEQueue<probe_t> probes_4;   // most recently received probes
    DEQueue<probe_t> probes_11;   // most recently received probes
    DEQueue<probe_t> probes_22;   // most recently received probes
    probe_list_t(const IPAddress &p, unsigned int per, unsigned int t) : 
      ip(p), 
      period(per), 
      tau(t),
      sent(0), 
      fwd_small(0), 
      fwd_2(0), 
      fwd_4(0), 
      fwd_11(0), 
      fwd_22(0)
    { }
    probe_list_t() : period(0), tau(0) { }

    int rev_rate(struct timeval start, int rate) {
      struct timeval now;
      struct timeval p = { tau / 1000, 1000 * (tau % 1000) };
      struct timeval earliest;
      click_gettimeofday(&now);
      timersub(&now, &p, &earliest);


      if (period == 0) {
	return 0;
      }
      int num = 0;
      DEQueue<probe_t> *probes;
      switch (rate) {
      case 0:
	probes = &probes_small;
	break;
      case 2:
	probes = &probes_2;
	break;
      case 4:
	probes = &probes_4;
	break;
      case 11:
	probes = &probes_11;
	break;
      case 22:
	probes = &probes_22;
	break;
      default:
	return 0;
      }
      if (!probes) {
	click_chatter("no probes!\n");
	return 0;
      }
      for (int i = probes->size() - 1; i >= 0; i--) {
	if (timercmp(&earliest, &((*probes)[i].when), <)) {
	  num++;
	} else {
	  break;
	}
      }

      int num_expected = tau / period;
      struct timeval since_start;
      struct timeval foo = start;
      timersub(&now, &foo, &since_start);
      int ms_since_start = since_start.tv_sec*1000 + since_start.tv_sec/1000;

      if (num_expected > (ms_since_start / period)) {
	num_expected = (ms_since_start / period);
      }

      if (num_expected > sent) {
	struct timeval since_last_rx;
	timersub(&now, &last_rx, &since_last_rx);
	int ms_since_rx = since_last_rx.tv_sec*1000 + since_last_rx.tv_sec/1000;
	num_expected = sent;
	if (period) {
	  num_expected += (ms_since_rx / period);
	}
	//click_chatter("sent %d, ms_since_rx %d, tau %d num_expected %d num %d",
	//sent, ms_since_rx, tau, num_expected, num);
      }
      if (!num_expected) {
	num_expected = 1;
      }
      return 100 * num / num_expected;

    }
  };

  // Per-sender map of received probes.
  typedef HashMap<IPAddress, probe_list_t> ProbeMap;
  ProbeMap _bcast_stats;

  typedef HashMap<EtherAddress, IPAddress> RevARP;
  RevARP _rev_arp;

  Vector <IPAddress> _neighbors;
  int _next_neighbor_to_ad;

  static String read_stats(Element *, void *);
  static String read_bcast_stats(Element *, void *);

  // handlers
  static String read_window(Element *, void *);
  static String read_period(Element *, void *);
  static String read_tau(Element *, void *);
  static int write_window(const String &, Element *, void *, ErrorHandler *);
  static int write_period(const String &, Element *, void *, ErrorHandler *);
  static int write_tau(const String &, Element *, void *, ErrorHandler *);
  
  void add_bcast_stat(IPAddress, const link_probe &);

  void send_probe_hook(int rate);
  void send_probe(unsigned int size, int rate);
  static void static_send_small_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(0); }
  static void static_send_2_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(2); }
  static void static_send_4_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(4); }
  static void static_send_11_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(11); }
  static void static_send_22_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(22); }


  Timer *_timer_small;
  Timer *_timer_2;
  Timer *_timer_4;
  Timer *_timer_11;
  Timer *_timer_22;

  struct timeval _next_small;
  struct timeval _next_2;
  struct timeval _next_4;
  struct timeval _next_11;
  struct timeval _next_22;

  bool _2hop_linkstate;
 public:


  int get_etx(IPAddress);
  int get_etx(int, int);

  void update_links(IPAddress ip);
  ETTStat();
  ~ETTStat();
  
  const char *class_name() const		{ return "ETTStat"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const                 { return "x/y"; }
  void notify_noutputs(int);
  
  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif

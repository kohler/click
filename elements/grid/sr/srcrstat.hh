#ifndef SRCRSTATHH
#define SRCRSTATHH

/*
 * =c
 * SrcrStat([I<KEYWORDS>])
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
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
CLICK_DECLS

class Timer;

class SrcrStat : public Element {

public:

  // Packet formats:

  // SrcrStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

  
  struct link_probe {
    unsigned short cksum;     // internet checksum
    unsigned short psz;       // total packet size, including eth hdr

    uint32_t ip;
    unsigned int seq_no;
    unsigned int period;      // period of this node's probe broadcasts, in msecs
    unsigned int tau;         // this node's loss-rate averaging period, in msecs
    unsigned int sent;        // how many probes this node has sent
    unsigned int num_links;   // number of grid_link_entry entries following

    link_probe() : cksum(0), psz(0), ip(0), seq_no(0), period(0), tau(0), sent(0), num_links(0) { }

  };
  
  struct link_entry {
    uint16_t fwd;
    uint16_t rev;
    uint32_t ip;
    link_entry() : fwd(0), rev(0), ip(0) { }
    link_entry(uint16_t __fwd, uint16_t __rev, IPAddress __ip) : fwd(__fwd), rev(__rev), ip(__ip.addr()) { }
  };

private:
  unsigned int _window; // sequence numbers
  unsigned int _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _probe_size; // bytes

  unsigned int _seq;
  uint32_t _sent;
  EtherAddress _eth;
  IPAddress _ip;
  class LinkTable *_link_table;
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
    int fwd;
    struct timeval last_rx;
    DEQueue<probe_t> probes;   // most recently received probes
    probe_list_t(const IPAddress &p, unsigned int per, unsigned int t) : ip(p), period(per), tau(t),
    sent(0), fwd(0) { }
    probe_list_t() : period(0), tau(0) { }

    int rev_rate(struct timeval start) {
      struct timeval now;
      struct timeval p = { tau / 1000, 1000 * (tau % 1000) };
      struct timeval earliest;
      click_gettimeofday(&now);
      timersub(&now, &p, &earliest);


      if (period == 0) {
	return 0;
      }
      int num = 0;
      for (int i = probes.size() - 1; i >= 0; i--) {
	if (timercmp(&earliest, &probes[i].when, <)) {
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

  static void static_send_hook(Timer *, void *e) { ((SrcrStat *) e)->send_hook(); }
  void send_hook();

  Timer *_send_timer;
  struct timeval _next_bcast;


 public:


  int get_etx(IPAddress);
  int get_etx(int, int);

  SrcrStat();
  ~SrcrStat();
  
  const char *class_name() const		{ return "SrcrStat"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const                 { return "x/y"; }
  void notify_noutputs(int);
  
  SrcrStat *clone() const;

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif

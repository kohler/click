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
 * =item USE_SECOND_PROTO
 *
 * Boolean.  If true, use the alternate LinkStat ethernet protocol
 * number.  The normal protocol number is 0x7ffe; the alternate number
 * is 0x7ffd.  Defaults to false.
 *
 * =back */

#include <click/bighashmap.hh>
#include <click/deque.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <elements/grid/timeutils.hh>

CLICK_DECLS

class Timer;

class LinkStat : public Element {

public:

  // build & extract network byte order values
  static unsigned uint_at(const unsigned char *c)
  { return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3]; }

  static unsigned ushort_at(const unsigned char *c)
  { return (c[0] << 8) | c[1]; }

  static void write_uint_at(unsigned char *c, unsigned u) {
    c[0] = (u >> 24) & 0xff; c[1] = (u >> 16) & 0xff;
    c[2] = (u >>  8) & 0xff; c[3] = u & 0xff;
  }

  static void write_ushort_at(unsigned char *c, unsigned u) {
    c[0] = (u >>  8) & 0xff; c[1] = u & 0xff;
  }

  // Packet formats:

  // LinkStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

#define FUCKED_GCC_2_96
#ifdef FUCKED_GCC_2_96
  enum {
    ETHERTYPE_LINKSTAT = 0x7ffe,
    ETHERTYPE_LINKSTAT2 = 0x7ffd
  };
#else
  static const unsigned short ETHERTYPE_LINKSTAT = 0x7ffe;
  static const unsigned short ETHERTYPE_LINKSTAT2 = 0x7ffd;
#endif

  struct link_probe {
    static const int size = 20;
    static const int cksum_offset = 16;

    unsigned int seq_no;
    unsigned int period;      // period of this node's probe broadcasts, in msecs
    unsigned int num_links;   // number of grid_link_entry entries following
    unsigned int tau;         // this node's loss-rate averaging period, in msecs
    unsigned short cksum;     // internet checksum
    unsigned short psz;       // total packet size, including eth hdr

    link_probe() : seq_no(0), period(0), num_links(0), tau(0), cksum(0), psz(0) { }
    link_probe(unsigned s, unsigned p, unsigned n, unsigned t, unsigned short sz)
      : seq_no(s), period(p), num_links(n), tau(t), cksum(0), psz(sz) { }

    // build link probe from wire format packet data
    link_probe(const unsigned char *);

    // write probe in wire format, return number of bytes written
    int write(unsigned char *) const;

    // update cksum of packet data whose link_probe starts at D
    static void update_cksum(unsigned char *d);
    static unsigned short calc_cksum(const unsigned char *d);
  };

  struct link_entry {
    static const int size = 8;

    class EtherAddress eth;
    unsigned short num_rx;    // number of probe bcasts received from node during last tau msecs

    link_entry() : num_rx(0) { }
    link_entry(const EtherAddress &e, unsigned short n) : eth(e), num_rx(n) { }
    link_entry(const unsigned char *);
    int write(unsigned char *) const;
  };

private:
  unsigned int _window; // sequence numbers
  unsigned int _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _probe_size; // bytes

  unsigned int _seq;
  EtherAddress _eth;


  // record probes received from other hosts
  struct probe_t {
    Timestamp when;
    unsigned seq_no;
    probe_t(const Timestamp &t, unsigned int s) : when(t), seq_no(s) { }
  };

  struct probe_list_t {
    EtherAddress    eth;
    unsigned int    period;   // period of this node's probes, as reported by the node
    unsigned int    tau;      // this node's stats averaging period, as reported by the node
    Deque<probe_t> probes;   // most recently received probes
    probe_list_t(const EtherAddress &e, unsigned int p, unsigned int t) : eth(e), period(p), tau(t) { }
    probe_list_t() : period(0), tau(0) { }
  };

  // Per-sender map of received probes.
  typedef HashMap<EtherAddress, probe_list_t> ProbeMap;
  ProbeMap _bcast_stats;

  // record delivery rate data about our outgoing links
  struct outgoing_link_entry_t : public link_entry {
    Timestamp received_at;
    unsigned  tau;
    outgoing_link_entry_t() { memset(this, 0, sizeof(*this)); }
    outgoing_link_entry_t(const link_entry &l, const Timestamp &now, unsigned int t)
      : link_entry(l), received_at(now), tau(t) { }
    outgoing_link_entry_t(const unsigned char *d, const Timestamp &now, unsigned int t)
      : link_entry(d), received_at(now), tau(t) { }
  };

  // Per-receiver map of delivery rate data
  typedef HashMap<EtherAddress, outgoing_link_entry_t> ReverseProbeMap;
  ReverseProbeMap _rev_bcast_stats;

  static String read_stats(Element *, void *);
  static String read_bcast_stats(Element *, void *);

  // count number of probes received from specified host during last
  // _tau msecs.
  unsigned int count_rx(const EtherAddress &);
  unsigned int count_rx(const probe_list_t *);

  // handlers
  static String read_window(Element *, void *);
  static String read_period(Element *, void *);
  static String read_tau(Element *, void *);
  static int write_window(const String &, Element *, void *, ErrorHandler *);
  static int write_period(const String &, Element *, void *, ErrorHandler *);
  static int write_tau(const String &, Element *, void *, ErrorHandler *);

  void add_bcast_stat(const EtherAddress &, const link_probe &);

  static void static_send_hook(Timer *, void *e) { ((LinkStat *) e)->send_hook(); }
  void send_hook();

  Timer *_send_timer;

  bool _use_proto2;

  static unsigned int calc_pct(unsigned tau, unsigned period, unsigned num_rx);

 public:

  // Get forward delivery rate R from this node to node ETH over
  // period TAU milliseconds, as recorded at time T.  R is a
  // percentage (0-100).  Return true iff we have data.
  bool get_forward_rate(const EtherAddress &eth, unsigned int *r, unsigned int *tau,
			Timestamp *t);

  // Get reverse delivery rate R from node ETH to this node over
  // period TAU milliseconds, as of now.  R is a percentage 0-100.
  // Return true iff we have good data.
  bool get_reverse_rate(const EtherAddress &eth, unsigned int *r, unsigned int *tau);

  unsigned get_probe_size() const { return _probe_size; }

  LinkStat() CLICK_COLD;
  ~LinkStat() CLICK_COLD;

  const char *class_name() const		{ return "LinkStat"; }
  const char *port_count() const		{ return "1/0-1"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const                 { return "x/y"; }

  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);
};

template <class T> void grid_swap(T &a, T &b) {
    T t = a;
    a = b;
    b = t;
}

CLICK_ENDDECLS
#endif

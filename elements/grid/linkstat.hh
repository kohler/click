#ifndef LINKSTATHH
#define LINKSTATHH

/*
 * =c
 * LinkStat(AiroInfo, WINDOW [, ETH, IP, PERIOD])
 * =s Grid
 * =d
 *
 * Expects Grid ethernet packets as input.  Incoming packets are
 * passed through to the first output.  For each incoming packet,
 * queries the AiroInfo element and records information about the
 * packet sender's latest transmission stats (quality, signal
 * strength).  Records Grid probe packets received during the last
 * WINDOW milliseconds.  If a second output is connected, sends probe
 * packets every PERIOD milliseconds.  PERIOD defaults to 1000 (1
 * second).  The source Ethernet and IP addresses (ETH and IP) must be
 * supplied if the second output is connected.
 *
 * Statistics are made available to the PingPong element for sending
 * back to the transmitter's LinkTracker.
 *
 * =a
 * AiroInfo, LinkTracker, PingPong */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>

#include "airoinfo.hh"
CLICK_DECLS

class Timer;
class grid_link_probe;

class LinkStat : public Element {
private:
  AiroInfo *_ai;
  unsigned int _window; // msecs
  unsigned int _period; // msecs

  unsigned int _seq;
  IPAddress    _ip;
  EtherAddress _eth;

  struct probe_t {
    struct timeval when;  
    unsigned int   seq_no;  
    probe_t(const struct timeval &t, unsigned int s) : when(t), seq_no(s) { }
  };

  struct probe_list_t {
    unsigned int    ip;
    unsigned int    period;   // period of this node's probes, as reported by the node
    unsigned int    window;   // this node's stats window, as reported by the node
    Vector<probe_t> probes;   // probes received in the _window msecs before most recent probe
    probe_list_t(unsigned int ip_, unsigned int p, unsigned int w) : ip(ip_), period(p), window(w) { }
    probe_list_t() : ip(0), period(0), window(0) { }
  };

  // Per-sender map of received probes.
  BigHashMap<EtherAddress, probe_list_t> _bcast_stats;

  static String read_stats(Element *, void *);
  static String read_bcast_stats(Element *, void *);
  
  static String read_window(Element *, void *);
  static int write_window(const String &, Element *, void *, ErrorHandler *);
  
  void add_bcast_stat(const EtherAddress &, const IPAddress &, const grid_link_probe *);


  struct stat_t {
    int            qual;
    int            sig;
    int            noise;
    struct timeval when;
  };

  // Per-sender map of the most recent signal stats.
  BigHashMap<EtherAddress, stat_t> _stats;

  static void static_send_hook(Timer *, void *e) { ((LinkStat *) e)->send_hook(); }
  void send_hook();

  Timer *_send_timer;

 public:
  /*
   * look up most recent stats for E.  LAST is the local rx time of
   * the last packet used to collect stats for E, NUM_RX is the number
   * actually received during the interval [LAST - WINDOW, LAST], WINDOW
   * is the interval size in milliseconds, and NUM_EXPECTED is the
   * number we think should have seen, based on observed sequence
   * numbers.  Returns true if we have some data, else false.  
   */
  bool get_bcast_stats(const EtherAddress &e, struct timeval &last, unsigned int &window,
		       unsigned int &num_rx, unsigned int &num_expected);

  void remove_all_stats(const EtherAddress &e);

  LinkStat();
  ~LinkStat();
  
  const char *class_name() const		{ return "LinkStat"; }
  const char *processing() const		{ return "a/a"; }
  void notify_noutputs(int);
  
  LinkStat *clone() const;

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif

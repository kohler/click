#ifndef ETTSTATHH
#define ETTSTATHH

/*
=c
ETTStat([I<KEYWORDS>])

=s Wifi, Wireless Routing

Track broadcast loss rates at different bitrates.

=d

Expects probe packets as input.  Records the last WINDOW unique
(not neccessarily sequential) sequence numbers of link probes from
each host, and calculates loss rates over the last TAU milliseconds
for each host.  If the output is connected, sends probe
packets every PERIOD milliseconds.  The source Ethernet 
address ETH must be specified if the second output is
connected.

This element is a disaster and needs to be rewritten.

Keyword arguments are:

=over 8

=item ETH
Ethernet address of this node; required if output is connected.

=item PERIOD
Unsigned integer.  Millisecond period between sending link probes
if second output is connected.  Defaults to 1000 (1 second).

=item WINDOW
Unsigned integer.  Number of most recent sequence numbers to remember
for each host.  Defaults to 100.

=item TAU
Unsigned integer.  Millisecond period over which to calculate loss
rate for each host.  Defaults to 10,000 (10 seconds).

 =back

=a LinkStat
*/

#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <elements/wifi/arptable.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
CLICK_DECLS

class Timer;


class RateSize {
public:
  int _rate;
  int _size;
  RateSize(int r, int s): _rate(r), _size(s) { };
  
  inline bool
  operator==(RateSize other)
  {
    return (other._rate == _rate && other._size == _size);
  }
};

static const uint8_t _ett_version = 0x02;

class ETTStat : public Element {

public:

  // Packet formats:

  // ETTStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

  enum {
    PROBE_AVAILABLE_RATES = (1<<0),
  };

  
  struct link_probe {
    uint8_t _version;
    unsigned short _cksum;     // internet checksum
    unsigned short _psz;       // total packet size, including eth hdr

    uint16_t _rate;
    uint16_t _size;
    uint32_t _ip;
    uint32_t _flags;
    unsigned int _seq;
    unsigned int _period;      // period of this node's probe broadcasts, in msecs
    uint32_t _tau;         // this node's loss-rate averaging period, in msecs
    unsigned int _sent;        // how many probes this node has sent
    unsigned int _num_probes;
    unsigned int _num_links;   // number of wifi_link_entry entries following

    link_probe() { memset(this, 0x0, sizeof(this)); }

  };
  
  struct link_info {
    uint16_t _size;
    uint8_t _rate;
    uint8_t _fwd;
    uint8_t _rev;
  };
  struct link_entry {
    uint32_t _ip;
    uint8_t _num_rates;
    uint32_t _seq;
    uint32_t _age;
    link_entry() { }
    link_entry(IPAddress ip) : _ip(ip.addr()) { }
  };

public:
  uint32_t _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _seq;
  uint32_t _sent;
  EtherAddress _eth;

  IPAddress _ip;
  IPAddress reverse_arp(EtherAddress);
  void take_state(Element *, ErrorHandler *);
  Vector<IPAddress> get_neighbors() {
    return _neighbors;
  }

  class ETTMetric *_ett_metric;
  class TXCountMetric *_etx_metric;
  class ARPTable *_arp_table;
  uint16_t _et;     // This protocol's ethertype

  struct timeval _start;
  // record probes received from other hosts
  struct probe_t {
    struct timeval _when;  
    uint32_t   _seq;
    uint8_t _rate;
    uint16_t _size;
    probe_t(const struct timeval &t, 
	    uint32_t s,
	    uint8_t r,
	    uint16_t sz) : _when(t), _seq(s), _rate(r), _size(sz) { }
  };


  struct probe_list_t {
    IPAddress _ip;
    int _period;   // period of this node's probes, as reported by the node
    uint32_t _tau;      // this node's stats averaging period, as reported by the node
    int _sent;
    int _num_probes;
    uint32_t _seq;
    Vector<RateSize> _probe_types;
    
    Vector<int> _fwd_rates;
    
    struct timeval _last_rx;
    DEQueue<probe_t> _probes;   // most recently received probes
    probe_list_t(const IPAddress &p, unsigned int per, unsigned int t) : 
      _ip(p), 
      _period(per), 
      _tau(t),
      _sent(0)
    { }
    probe_list_t() : _period(0), _tau(0) { }

    int rev_rate(struct timeval start, int rate, int size) {
      struct timeval now;
      struct timeval p = { _tau / 1000, 1000 * (_tau % 1000) };
      struct timeval earliest;
      click_gettimeofday(&now);
      timersub(&now, &p, &earliest);


      if (_period == 0) {
	click_chatter("period is 0\n");
	return 0;
      }
      int num = 0;
      for (int i = _probes.size() - 1; i >= 0; i--) {
	if (timercmp(&earliest, &(_probes[i]._when), >)) {
	  break;
	} 
	if ( _probes[i]._size == size &&
	    _probes[i]._rate == rate) {
	  num++;
	}
      }
      
      struct timeval since_start;
      timersub(&now, &start, &since_start);

      uint32_t ms_since_start = MAX(0, since_start.tv_sec * 1000 + since_start.tv_usec / 1000);
      uint32_t fake_tau = MIN(_tau, ms_since_start);
      assert(_probe_types.size());
      int num_expected = fake_tau / _period;

      if (_sent / _num_probes < num_expected) {
	num_expected = _sent / _num_probes;
      }
      if (!num_expected) {
	num_expected = 1;
      }

      return MIN(100, 100 * num / num_expected);

    }
  };

public:
  // Per-sender map of received probes.
  typedef HashMap<IPAddress, probe_list_t> ProbeMap;
  ProbeMap _bcast_stats;

  typedef HashMap<EtherAddress, IPAddress> RevARP;
  RevARP _rev_arp;

  Vector <IPAddress> _neighbors;
  int _next_neighbor_to_ad;


  void add_bcast_stat(IPAddress, const link_probe &);
  
  void update_link(IPAddress from, IPAddress to, Vector<RateSize> rs, Vector<int> fwd, Vector<int> rev, uint32_t seq);
  void send_probe_hook();
  void send_probe();
  static void static_send_hook(Timer *, void *e) { ((ETTStat *) e)->send_probe_hook(); }

  Timer *_timer;
  Timer _stale_timer;

  void run_timer();
  struct timeval _next;

  Vector <RateSize> _ads_rs;
  int _ads_rs_index;


  class AvailableRates *_rtable;


    typedef HashMap<EtherAddress, uint8_t> BadTable;
  typedef BadTable::const_iterator BTIter;
  
  BadTable _bad_table;

 public:
  String bad_nodes();
  String read_bcast_stats();


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

  void reset();
  void clear_stale();
  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif

#ifndef CLICK_GatewaySelector_HH
#define CLICK_GatewaySelector_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include "srcr.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * GatewaySelector(IP, ETH, ETHERTYPE, [LS link-state-element])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: ethernet data packets from device 
 * Input 2: IP packets from higher layer, w/ ip addr anno.
 * Input 3: IP packets from higher layer for gw, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class GatewaySelector : public Element {
 public:
  
  GatewaySelector();
  ~GatewaySelector();
  
  const char *class_name() const		{ return "GatewaySelector"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  GatewaySelector *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();

  static String static_print_stats(Element *e, void *);
  String print_stats();

  static String static_print_current_gateway(Element *e, void *);
  String print_current_gateway();

  static String static_print_is_gateway(Element *e, void *);
  String print_is_gateway();

  static int static_write_is_gateway(const String &arg, Element *el,
				     void *, ErrorHandler *errh);
  void write_is_gateway(bool b);

  void push(int, Packet *);
  void run_timer();

  int get_metric(IPAddress other);
  void update_link(IPAddress from, IPAddress to, int metric);
  void forward_ad_hook();
private:
    // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _gw;
    u_long _seq;
    int _metric;
    int _count;
    struct timeval _when; /* when we saw the first query */
    struct timeval _to_send;
    bool _forwarded;
    Vector<IPAddress> _hops;
    Vector<int> _metrics;
    Seen(IPAddress gw, u_long seq, int metric) {
      _gw = gw; _seq = seq; _count = 0; _metric = metric;
    }
    Seen();
  };
  
  DEQueue<Seen> _seen;
  
  class GWInfo {
  public:
    IPAddress _ip;
    struct timeval _last_update;
    GWInfo() {memset(this,0,sizeof(*this)); }
  };

  typedef BigHashMap<IPAddress, GWInfo> GWTable;
  typedef GWTable::const_iterator GWIter;
  GWTable _gateways;

  IPAddress _current_gateway;



  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  struct timeval _gw_expire;
  u_long _seq;      // Next query sequence number to use.
  int _warmup;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype
  struct timeval _warmup_expire;
  unsigned int _period; // msecs

  EtherAddress _bcast;
  bool _is_gw;

  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  class ARPTable *_arp_table;
  Timer _timer;





  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  void got_sr_pkt(Packet *p_in);
  void start_ad();
  void process_query(struct sr_pkt *pk);
  void forward_ad(Seen *s);
  void send(WritablePacket *);
  void process_data(Packet *p_in);
  bool pick_new_gateway();
  bool valid_gateway(IPAddress);
  void gatewayselector_assert_(const char *, int, const char *) const;
  static void static_forward_ad_hook(Timer *, void *e) { 
    ((GatewaySelector *) e)->forward_ad_hook(); 
  }
};


CLICK_ENDDECLS
#endif

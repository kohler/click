#ifndef CLICK_TOP5_HH
#define CLICK_TOP5_HH
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
#include "path.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * Top5(IP, ETH, ETHERTYPE, [METRIC GridGenericMetric])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: ethernet data packets destined to device
 * Input 2: IP packets from higher layer, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class Top5 : public Element {
 public:
  
  Top5();
  ~Top5();
  
  const char *class_name() const		{ return "Top5"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  Top5 *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  static int static_start(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void start(IPAddress dst);



  static String static_print_stats(Element *e, void *);
  String print_stats();

  void push(int, Packet *);
  void run_timer();

  static unsigned int jiff_to_ms(unsigned int j)
  { return (j * 1000) / CLICK_HZ; }

  static unsigned int ms_to_jiff(unsigned int m)
  { return (CLICK_HZ * m) / 1000; }

  int get_metric(IPAddress other);
  void update_link(IPAddress from, IPAddress to, int metric);
  void forward_query_hook();
  void start_reply_hook();
private:

#define TOP_N 5
  class Query {
  public:
    Query() {memset(this, 0, sizeof(*this)); }
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    int _metric;
    Path _p;
    Vector<int> _metrics;
    struct timeval _to_send;
    bool _forwarded;
  };

  class Reply {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    struct timeval _to_send;
    bool _sent;
  };
  
  class Dst {
  public:
    IPAddress _ip;
    Vector<Path> _paths;
    Vector<int> _count;
    int _current_path;
    bool _started;
  };

  typedef HashMap<IPAddress, Dst> DstTable;
  DstTable _dsts;
  DEQueue<Reply> _replies;


  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    int _metric;
    int _count;
    struct timeval _when; /* when we saw the first query */
    struct timeval _to_send;
    bool _forwarded;
    Vector<IPAddress> _hops;
    Vector<int> _metrics;
    Vector<IPAddress> _extra_hosts;
    Vector<int> _extra_metrics;

    Seen(IPAddress src, IPAddress dst, u_long seq, int metric) {
      _src = src; _dst = dst; _seq = seq; _count = 0; _metric = metric;
    }
    Seen();
  };


  typedef HashMap<IPAddress, Query> QueryTable;
  QueryTable _queries;

  typedef BigHashMap<IPAddress, bool> IPMap;

  DEQueue<Seen> _seen;

  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  struct timeval _query_wait;
  struct timeval _rev_path_update;
  u_long _seq;      // Next query sequence number to use.
  uint32_t _et;     // This protocol's ethertype
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  Timer _timer;

  IPAddress _bcast_ip;

  EtherAddress _bcast;

  class SRForwarder *_sr_forwarder;
  class LinkTable *_link_table;
  class SrcrStat *_srcr_stat;
  class ARPTable *_arp_table;

  // Statistics for handlers.
  int _num_queries;
  int _bytes_queries;
  int _num_replies;
  int _bytes_replies;







  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  void got_srpacket(Packet *p_in);
  void start_query(IPAddress);
  void process_query(struct srpacket *pk);
  void forward_query(Seen *s);
  void start_reply(Reply *r);
  void forward_reply(struct srpacket *pk);
  void got_reply(struct srpacket *pk);
  void start_data(const u_char *data, u_long len, Vector<IPAddress> r);
  void send(WritablePacket *);
  void process_data(Packet *p_in);
  void top5_assert_(const char *, int, const char *) const;
  static void static_forward_query_hook(Timer *, void *e) { 
    ((Top5 *) e)->forward_query_hook(); 
  }
  static void static_start_reply_hook(Timer *, void *e) { 
    ((Top5 *) e)->start_reply_hook();
  }
};


CLICK_ENDDECLS
#endif

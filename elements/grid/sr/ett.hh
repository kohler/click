#ifndef CLICK_ETT_HH
#define CLICK_ETT_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include "srcr.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * EETT(IP, ETH, ETHERTYPE, [LS link-state-element])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: IP packets from higher layer, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class ETT : public Element {
 public:
  
  ETT();
  ~ETT();
  
  const char *class_name() const		{ return "ETT"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  ETT *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  static int static_start(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void start(IPAddress dst);



  static int static_link_failure(const String &arg, Element *e,
				 void *, ErrorHandler *errh); 
  void link_failure(EtherAddress dst);



  static String static_print_stats(Element *e, void *);
  String print_stats();

  static String static_print_current_gateway(Element *e, void *);
  String print_current_gateway();

  static String static_print_is_gateway(Element *e, void *);
  String print_is_gateway();


  void push(int, Packet *);
  void run_timer();

  static unsigned int jiff_to_ms(unsigned int j)
  { return (j * 1000) / CLICK_HZ; }

  static unsigned int ms_to_jiff(unsigned int m)
  { return (CLICK_HZ * m) / 1000; }

  int get_metric(IPAddress other);
  void update_link(IPAddress from, IPAddress to, int metric);
  static String route_to_string(Vector<IPAddress> s);
private:

  class Query {
  public:
    Query() {memset(this, 0, sizeof(*this)); }
    Query(IPAddress ip) {memset(this, 0, sizeof(*this)); _ip = ip;}
    IPAddress _ip;
    u_long _seq;
    u_short _metric;
    struct timeval _last_query;

  };

  
  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    int _metric;
    int _count;
    struct timeval _when; /* when we saw the first query */
    Seen(IPAddress src, IPAddress dst, u_long seq, int metric) {
      _src = src; _dst = dst; _seq = seq; _count = 0; _metric = metric;
    }
    Seen();
  };

  typedef HashMap<IPAddress, Query> QueryTable;
  QueryTable _queries;

  Vector<Seen> _seen;

  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  struct timeval _reply_wait;
  struct timeval _query_wait;

  u_long _seq;      // Next query sequence number to use.
  Timer _timer;
  int _warmup;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype
  int _warmup_period;

  IPAddress _gw;
  IPAddress _bcast_ip;

  EtherAddress _bcast;
  bool _is_gw;

  class SRCR *_srcr;
  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  class ARPTable *_arp_table;

  // Statistics for handlers.
  int _num_queries;
  int _bytes_queries;
  int _num_replies;
  int _bytes_replies;







  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  void got_sr_pkt(Packet *p_in);
  void start_query(IPAddress);
  void process_query(struct sr_pkt *pk);
  void forward_query(Seen s, Vector<IPAddress> hops, Vector<int> rev_metrics);
  void start_reply(IPAddress src, IPAddress dst, u_long seq);
  void forward_reply(struct sr_pkt *pk);
  void got_reply(struct sr_pkt *pk);
  void start_data(const u_char *data, u_long len, Vector<IPAddress> r);
  void send(WritablePacket *);

  void ett_assert_(const char *, int, const char *) const;

};


CLICK_ENDDECLS
#endif

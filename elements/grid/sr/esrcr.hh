#ifndef CLICK_ESRCR_HH
#define CLICK_ESRCR_HH
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
CLICK_DECLS

/*
 * =c
 * EESRCR(IP, ETH, ETHERTYPE, [LS link-state-element])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: IP packets from higher layer, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class ESRCR : public Element {
 public:
  
  ESRCR();
  ~ESRCR();
  
  const char *class_name() const		{ return "ESRCR"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  ESRCR *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  static int static_start_query(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void start_query(IPAddress);

  static String static_print_stats(Element *e, void *);
  String print_stats();


  void push(int, Packet *);
  void run_timer();



  static unsigned int jiff_to_ms(unsigned int j)
  { return (j * 1000) / CLICK_HZ; }

  static unsigned int ms_to_jiff(unsigned int m)
  { return (CLICK_HZ * m) / 1000; }


  static String ESRCR::route_to_string(Vector<IPAddress> s);
private:
  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.


  struct timeval _reply_wait;

  u_long _seq;      // Next query sequence number to use.
  Timer _timer;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype

  EtherAddress _bcast;
  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  IPAddress _ls_net;
  class ARPTable *_arp_table;
  // State of a destination.
  // We might have a request outstanding for this destination.
  class Dst {
  public:
    Dst() {_ip = IPAddress() ; _seq = 0; }
    Dst(IPAddress ip) { _ip = ip; _seq = 0;}
    IPAddress _ip;
    u_long _seq;
  };

  class Src {
  public:
    Src() {_ip = IPAddress(); _seq = 0; }
    Src(IPAddress ip, u_long seq) { _ip = ip; _seq = seq;}
    IPAddress _ip;
    u_long _seq;
    struct timeval _when;
  };

  typedef HashMap<IPAddress, Dst> DstTable;
  DstTable _dsts;

  typedef HashMap<IPAddress, Src> SrcTable;
  SrcTable _srcs;

  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    int _count;
    Seen(IPAddress src, IPAddress dst, u_long seq ) {
      _src = src; _dst = dst; _seq = seq; _count = 0;
    }
    Seen();
  };
  Vector<Seen> _seen;



  static void static_reply_hook(Timer *t, void *v) 
   { ((ESRCR *) v)->reply_hook(t); }


  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  u_short get_metric(IPAddress other);
  void got_sr_pkt(Packet *p_in);
  void process_query(struct sr_pkt *pk);
  void forward_query(Seen s, Vector<IPAddress> hops, Vector<u_short> metrics);
  void start_reply(IPAddress src);
  void forward_reply(struct sr_pkt *pk);
  void got_reply(struct sr_pkt *pk);
  void start_data(const u_char *data, u_long len, Vector<IPAddress> r);
  void send(WritablePacket *);

  void reply_hook(Timer *t);
  void update_best_metrics();
  void update_link(IPPair p, u_short m, unsigned int now);
  void esrcr_assert_(const char *, int, const char *) const;

  // Statistics for handlers.
  int _queries;
  int _querybytes;
  int _replies;
  int _replybytes;


};


CLICK_ENDDECLS
#endif

#ifndef CLICK_SRCR_HH
#define CLICK_SRCR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "linktable.hh"
CLICK_DECLS

/*
 * =c
 * SRCR(IP, ETH, ETHERTYPE, [LS link-state-element])
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: IP packets from higher layer, w/ ip addr anno.
 * Input 1: ethernet packets.
 * Output 0: IP packets for higher layer.
 * Output 1: ethernet packets.
 * Test script in ~rtm/scripts/srcr.pl
 *
 * To Do:
 * Delete or re-check old routes? (Maybe not, maybe wait for failure.)
 * Work sensibly with multiple network interfaces.
 * Save the packet we're querying for, like ARP does.
 * Signal broken links &c.
 * Reliability of the unicast REPLY would be a serious problem
 *   were it not for 802.11 ACK.
 * How to use a metric effectively?
 *   1) Let any better new route through.
 *   2) Stall every query long enough to hear from all neighbors.
 *      Has trouble with longer hop-count paths.
 *   3) Unicast to neighbors in order of link quality.
 *   4) Stall every query proportional to link quality from
 *      neighbor we heard it from.
 * Two distinct issues: A) how to make sure we deliver the best
 *   query+route, and B) how to avoid forwarding an exponential
 *   number of queries.
 * Accumulate current path quality in each packet, re-query if it's
 *   too much worse that original quality.
 * Be aware if path to some other destination reveals potentially
 *   useful better links.
 * If you learn about multiple disjoint paths, use them for multi-path
 *   routing. May need to know about link speed to reason correctly
 *   about links shared by two paths.
 * Be sensitive to congestion? If dst receives packets out of order along
 *   different paths, prefer paths that delivered packets first?
 *   Maybe use expected transmission *time*.
 * Observations on the indoor Grid net (w/o "forward if better"):
 *   Queries often arrive out of order -- longer paths first.
 *   Also replies along different paths arrive out of order.
 *   Very common to end up with one direction sub-optimal e.g.
 *     [m=430 1.0.0.26 1.0.0.18 1.0.0.19 1.0.0.37]
 *     [m=273 1.0.0.37 1.0.0.28 1.0.0.26]
=e
kt :: KernelTun(1.0.0.1/24);
ls :: LinkStat(ETH 00:20:e0:8b:5d:d6, IP 1.0.0.1);
dsr :: SRCR(1.0.0.1, 00:20:e0:8b:5d:d6, 0x0807, LS ls);
fd :: FromDevice(wi0, 0);
td :: ToDevice(wi0);
kt -> icl :: Classifier(12/080045, -);
icl[0] -> Strip(14) -> [0]dsr;
icl[1] -> [0]dsr;
dsr[0] -> CheckIPHeader -> kt;
fd -> ncl :: Classifier(12/0807, 12/7fff);
ncl[0] -> [1]dsr;
dsr[1] -> td;
ncl[1] -> ls;
ls -> td;
 */


enum DSRPacketType { PT_QUERY = 0x11,
		     PT_REPLY = 0x22,
		     PT_DATA  = 0x33 };

enum DSRPacketFlags { PF_BETTER = 1 };



// Packet format.
struct sr_pkt {
  uint8_t	ether_dhost[6];
  uint8_t	ether_shost[6];
  uint16_t	ether_type;
  
  u_char _type;  // PacketType
  u_char _flags; // PacketFlags
  
  // PT_QUERY
  in_addr _qdst; // Who are we looking for?
  u_long _seq;   // Originator's sequence number.
  
  // PT_REPLY
  // The data is in the PT_QUERY fields.
  
  // PT_DATA
  u_short _dlen;
  
  // Route
  u_short _nhops;
  u_short _next;   // Index of next node who should process this packet.
  
  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(ntohs(_nhops)); }
  size_t hlen_with_data() const { return len_with_data(ntohs(_nhops), ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct sr_pkt) + nhops * sizeof(in_addr) + nhops * sizeof(u_short);
  }
  static size_t len_with_data(int nhops, int dlen) {
    return len_wo_data(nhops) + dlen;
  }
  
  u_short num_hops() {
    return ntohs(_nhops);
  }
  
  /* yes, I'm that nasty */
  in_addr get_hop(int h) { 
    in_addr *ndx = (in_addr *) (ether_dhost + sizeof(struct sr_pkt));
    return ndx[h];
  }
  u_short get_metric(int h) { 
    u_short *ndx = (u_short *) (ether_dhost + sizeof(struct sr_pkt) + _nhops * sizeof(in_addr));
    return ntohs(ndx[h]);
  }
  
  void  set_hop(int hop, in_addr s) { 
    in_addr *ndx = (in_addr *) (ether_dhost + sizeof(struct sr_pkt));
    ndx[hop] = s;
  }
  void set_metric(int hop, u_short s) { 
    u_short *ndx = (u_short *) (ether_dhost + sizeof(struct sr_pkt) + _nhops * sizeof(in_addr));
    ndx[hop] = htons(s);
  }
  
  u_char *data() { return ether_dhost + len_wo_data(_nhops); }
  String s();
};


class SRCR : public Element {
 public:
  
  SRCR();
  ~SRCR();
  
  const char *class_name() const		{ return "SRCR"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  SRCR *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void add_handlers();
  
  void push(int, Packet *);
  void run_timer();
  static timeval get_timeval(void);
  static timeval add_millisec(timeval t, int milli);
  static bool timeval_past(timeval a, timeval b); // return if a is past b
  static String SRCR::route_to_string(Vector<IPAddress> s);
  // Statistics for handlers.
  int _queries;
  int _querybytes;
  int _replies;
  int _replybytes;
  int _datas;
  int _databytes;

private:
  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  int QueryInterval; // Don't re-query a dead dst too often.
  int QueryLife; // Forget already-seen queries this often.
  int ARPLife;   // ARP cache timeout.
  
  u_long _seq;      // Next query sequence number to use.
  Timer _timer;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype.
  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  
  // State of a destination.
  // We might have a request outstanding for this destination.
  // We might know some routes to this destination.
  class Dst {
  public:
    Dst(IPAddress ip) { _ip = ip; _seq = 0; _when.tv_sec = 0; _when.tv_usec = 0; }
    IPAddress _ip;
    u_long _seq; // Of last query sent out.
    timeval _when; // When we sent last query.
  };

  Vector<Dst> _dsts;

  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    timeval _when;
    u_short _metric;
    Vector<u_short> _metrics; // The hop-by-hop
    Vector<IPAddress> _hops;  // the best route seen for this <src, dst, seq>
    u_short _nhops;
    bool _forwarded;
    Seen(IPAddress src, IPAddress dst, u_long seq, timeval now, 
	 u_short metric, Vector<u_short> metrics, Vector<IPAddress> hops, u_short nhops) {
      _src = src; _dst = dst; _seq = seq; _metric = metric; _metrics = metrics; 
      _when = now; _hops = hops; _nhops = nhops; _forwarded = false;
    }
    String s();
    
  };
  Vector<Seen> _seen;

  // Poor man's ARP cache. We're layered under IP, so cannot
  // use ordinary ARP. Just keep track of IP/ether mappings
  // we happen to hear of.
  class ARP {
  public:
    IPAddress _ip;
    EtherAddress _en;
    timeval _when; // When we last heard from this node.
    ARP(IPAddress ip, EtherAddress en, timeval now) {
      _ip = ip; _en = en; _when = now;
    }
  };
  Vector<ARP> _arp;

  static void static_query_hook(Timer *t, void *v) 
   { ((SRCR *) v)->query_hook(t); }


  int find_dst(IPAddress ip, bool create);
  bool find_arp(IPAddress ip, u_char en[6]);
  void got_arp(IPAddress ip, u_char xen[6]);
  u_short get_metric(IPAddress other);
  void got_sr_pkt(Packet *p_in);
  void start_query(IPAddress);
  void process_query(struct sr_pkt *pk);
  void forward_query(Seen s);
  void start_reply(struct sr_pkt *pk1);
  void forward_reply(struct sr_pkt *pk);
  void got_reply(struct sr_pkt *pk);
  void start_data(const u_char *data, u_long len, Vector<IPAddress> r);
  void got_data(struct sr_pkt *pk);
  void forward_data(struct sr_pkt *pk);
  void send(WritablePacket *);

  void query_hook(Timer *t);

  void dsr_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif

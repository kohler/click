#ifndef CLICK_RTMDSR_HH
#define CLICK_RTMDSR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
CLICK_DECLS

/*
 * =c
 * RTMDSR(IP, ETH, ETHERTYPE)
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: packets from higher layer (probably IP), w/ ip addr anno.
 * Input 1: ethernet packets.
 * Output 0: packets for higher layer (probably IP).
 * Output 1: ethernet packets.
 *
 * To Do:
 * Need something like ARP to avoid broadcasting data, and to get ACKs
 * Assumes just one network interface.
 * Save the packet we're querying for, like ARP does.
 * Signal broken links &c.
 * Integrate with tx count machinery.
=e
kt :: KernelTun(1.0.0.1/24);
dsr :: RTMDSR(1.0.0.1);
fd :: FromDevice(wi0, 1);
td :: ToDevice(wi0);
kt -> [0]dsr;
dsr[0] -> CheckIPHeader -> kt;
fd -> Classifier(12/0807) -> Strip(14) -> [1]dsr;
dsr[1]
  -> EtherEncap(0x0807, 1:2:3:4:5:6, ff:ff:ff:ff:ff:ff)
  -> td;
dsr[2]
  -> EtherEncap(0x0807, 1:2:3:4:5:6, ff:ff:ff:ff:ff:ff)
  -> td;
 */

class RTMDSR : public Element {
 public:
  
  RTMDSR();
  ~RTMDSR();
  
  const char *class_name() const		{ return "RTMDSR"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  RTMDSR *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);
  
  void push(int, Packet *);
  void run_timer();
  static time_t time(void);

private:
  static const int
    MaxSeen = 200,  // Max size of table of already-seen queries.
    MaxHops = 30,   // Max hop count for queries.
    QueryInterval = 10, // Don't re-query a dead dst too often.
    QueryLife = 3,  // Forget already-seen queries this often.
    ARPLife = 30;   // ARP cache timeout.

  Timer _timer;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype.

  enum PacketType { PT_QUERY = 0x01010101,
                    PT_REPLY = 0x02020202,
                    PT_DATA  = 0x03030303 };

  // Packet format.
  struct pkt {
    uint8_t	ether_dhost[6];
    uint8_t	ether_shost[6];
    uint16_t	ether_type;

    u_long _type; // PacketType

    // PT_QUERY
    in_addr _qdst; // Who are we looking for?
    u_long _seq;     // Originator's sequence number.
    
    // PT_REPLY
    // The data is in the PT_QUERY fields.

    // PT_DATA
    u_short _dlen;

    // Route
    u_short _nhops;
    u_short _next;   // Index of next node who should process this packet.
    in_addr _hops[];

    // How long should the packet be?
    size_t hlen() const { return hlen1(ntohs(_nhops)); }
    size_t len() const { return len1(ntohs(_nhops), ntohs(_dlen)); }
    static size_t hlen1(int nhops) {
      return sizeof(struct pkt) + nhops * sizeof(in_addr);
    }
    static size_t len1(int nhops, int dlen) {
      return hlen1(nhops) + dlen;
    }
    u_char *data() { return ether_dhost + hlen(); }
  };

  // Description of a single hop in a route.
  class Hop {
  public:
    IPAddress _ip;
    Hop(IPAddress ip) { _ip = ip; }
  };

  // Description of a route to a destination.
  class Route {
  public:
    time_t _when; // When we learned about this route.
    int _pathmetric;
    Vector<Hop> _hops;
    String s();
    Route() { _when = 0; _pathmetric = 9999; };
  };

  // State of a destination.
  // We might have a request outstanding for this destination.
  // We might know some routes to this destination.
  class Dst {
  public:
    Dst(IPAddress ip) { _ip = ip; _seq = 0; _when = 0; }
    IPAddress _ip;
    u_long _seq; // Of last query sent out.
    time_t _when; // When we sent last query.
    Vector<Route> _routes;
  };

  Vector<Dst> _dsts;

  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    u_long _seq;
    time_t _when;
    Seen(IPAddress src, u_long seq) {
      _src = src; _seq = seq; _when = time();
    }
  };
  Vector<Seen> _seen;

  // Poor man's ARP cache. We're layered under IP, so cannot
  // use ordinary ARP. Just keep track of IP/ether mappings
  // we happen to hear of.
  class ARP {
  public:
    IPAddress _ip;
    EtherAddress _en;
    time_t _when; // When we last heard from this node.
    ARP(IPAddress ip, EtherAddress en) {
      _ip = ip; _en = en; _when = time();
    }
  };
  Vector<ARP> _arp;

  int find_dst(IPAddress ip, bool create);
  Route &best_route(IPAddress);
  bool find_arp(IPAddress ip, u_char en[6]);
  void got_arp(IPAddress ip, u_char xen[6]);
  void got_pkt(Packet *p_in);
  void start_query(IPAddress);
  void forward_query(struct pkt *pk);
  void start_reply(struct pkt *pk1);
  void forward_reply(struct pkt *pk);
  void got_reply(struct pkt *pk);
  void start_data(const u_char *data, u_long len, Route &r);
  void got_data(struct pkt *pk);
  void forward_data(struct pkt *pk);
  void send(WritablePacket *);
  void forward(const struct pkt *pk1);
};

CLICK_ENDDECLS
#endif

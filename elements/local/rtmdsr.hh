#ifndef CLICK_RTMDSR_HH
#define CLICK_RTMDSR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
CLICK_DECLS

/*
 * =c
 * RTMDSR(IP)
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Data packets from higher layer into input zero, with IP Dst
 * annotation set.
 * Protocol packets into input one.
 * Output zero goes to upper layers on this host.
 * Broadcast packets out output one.
 * Unicast packets out output two.
 *
 * To Do:
 * Assumes just one network interface.
 * Doesn't delete old items from already-seen cache.
 * =e
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
  Timer _timer;
  IPAddress _ip; // Our IP address.

  enum PacketType { PT_QUERY = 0x01010101,
                    PT_REPLY = 0x02020202,
                    PT_DATA  = 0x03030303 };

  // Packet format.
  struct pkt {
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
    size_t hlen() const { return hlen1(_nhops); }
    size_t len() const { return len1(_nhops, _dlen); }
    static size_t hlen1(int nhops) {
      return sizeof(struct pkt)
        + ntohs(nhops) * sizeof(in_addr);
    }
    static size_t len1(int nhops, int dlen) {
      return hlen1(nhops) + dlen;
    }
    u_char *data() { return ((u_char*)&_type) + hlen(); }
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

  int find_dst(IPAddress ip, bool create);
  Route &best_route(IPAddress);
  void got_pkt(Packet *p_in);
  void start_query(IPAddress);
  void forward_query(struct pkt *pk);
  void start_reply(struct pkt *pk1);
  void forward_reply(struct pkt *pk);
  void got_reply(struct pkt *pk);
  void start_data(const u_char *data, u_long len, Route &r);
  void got_data(struct pkt *pk);
  void forward_data(struct pkt *pk);
  void send(Packet *);
  void forward(const struct pkt *pk1);
};

CLICK_ENDDECLS
#endif

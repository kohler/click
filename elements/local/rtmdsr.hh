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
 * Broadcast packets out output zero.
 * Unicast packets out output one.
 * Assumes just one network interface.
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

private:
  Timer _timer;
  IPAddress _ip; // Our IP address.

  enum PacketType { PT_QUERY=0x01010101, PT_REPLY=0x02020202 };

  // Packet format.
  struct pkt {
    u_long _type; // PacketType

    // PT_QUERY
    in_addr _qdst; // Who are we looking for?
    u_long _seq;     // Originator's sequence number.
    
    // PT_REPLY
    // The data is in the PT_QUERY fields.

    // Route
    u_short _nhops;
    u_short _next;   // Index of next node who should process this packet.
    in_addr _hops[];

    size_t len() { return sizeof(struct pkt) +
                     ntohs(_nhops) * sizeof(_hops[0]); }
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

  int find_dst(IPAddress ip, bool create);
  Route &best_route(IPAddress);
  void RTMDSR::start_query(IPAddress);
  void got_pkt(Packet *p_in);
  time_t time(void);
  void send_reply(struct pkt *pk1);
  void forward_query(struct pkt *pk);
  bool already_seen(in_addr src, u_long seq);
  void got_reply(struct pkt *pk);
  void forward_reply(struct pkt *pk);
};

CLICK_ENDDECLS
#endif

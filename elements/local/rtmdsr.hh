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

  enum PacketType { PT_QUERY=90991 };

  // Packet format.
  struct pkt {
    PacketType _type;

    // PT_QUERY
    in_addr _dst; // Who are we looking for?
    int _metric;  // Path metric so far.
    int _seq;     // Originator's sequence number.
    
    // Route
    int _nhops;
    in_addr _hops[];

    int len() { return sizeof(struct pkt) +
                  ntohl(_nhops) * sizeof(in_addr); }
  };

  // Description of a single hop in a route.
  class Hop {
  public:
    IPAddress _ip;
    int _linkmetric;
  };

  // Description of a route to a destination.
  class Route {
  public:
    time_t _when; // When we learned about this route.
    int _pathmetric;
    Vector<Hop> _hops;
  };

  // State of a destination.
  // We might have a request outstanding for this destination.
  // We might know some routes to this destination.
  class Dst {
  public:
    Dst(IPAddress ip) { _ip = ip; _seq = 0; _when = 0; }
    IPAddress _ip;
    int _seq; // Of last query sent out.
    time_t _when; // When we sent last query.
    Vector<Route> _routes;
  };

  Vector<Dst> _dsts;

  Dst *find_dst(IPAddress ip, bool create);
  Route *best_route(Dst *d);
  void RTMDSR::start_query(Dst *d);
  void got_pkt(Packet *p_in);
  time_t time(void);
};

CLICK_ENDDECLS
#endif

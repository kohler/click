#ifndef CLICK_SETSOURCEROUTE_HH
#define CLICK_SETSOURCEROUTE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include "path.hh"
CLICK_DECLS

/*
 * =c
 * SetSourceRoute(IP, ETH, ETHERTYPE, SRCR element, LinkTable element, ARPtable element, 
 *    [METRIC GridGenericMetric], [WARMUP period in seconds])
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


class SetSourceRoute : public Element {
 public:
  
  SetSourceRoute();
  ~SetSourceRoute();
  
  const char *class_name() const		{ return "SetSourceRoute"; }
  const char *processing() const		{ return AGNOSTIC; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  Packet *simple_action(Packet *);

  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();


  static int static_set_route(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void set_route(Path p);

  static String static_print_routes(Element *e, void *);
  String print_routes();



private:

  typedef HashMap<IPAddress, Path> RouteTable;
  RouteTable _routes;

  IPAddress _ip;    // My IP address.

  class SRForwarder *_sr_forwarder;

};


CLICK_ENDDECLS
#endif

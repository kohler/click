#ifndef CLICK_SR2SETSOURCEROUTE_HH
#define CLICK_SR2SETSOURCEROUTE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/path.hh>
CLICK_DECLS

/*
=c

SR2SetSourceRoute(IP, SRForwarder element)

=s Wifi, Wireless Routing

=d

Set the Source Route for packet's destination inside the source route
header based on the destination ip annotation. If no source route is
found for a given packet, the unmodified packet is sent to output 1 if
the output is present.

Regular Arguments:
=over 8
=item IP
IPAddress
=item SRForwarder Element
=back 8


=h clear write
Removes all routes from this element

=h routes read
Prints routes 

=h set_route write
Writing "5.0.0.1 5.0.0.2 5.0.0.3" to this element will make
all packets destined for 5.0.0.3 from 5.0.0.1 to use a two-hop
route through 5.0.0.2.

=a SRForwarder
 */


class SR2SetSourceRoute : public Element {
 public:
  
  SR2SetSourceRoute();
  ~SR2SetSourceRoute();
  
  const char *class_name() const		{ return "SR2SetSourceRoute"; }
  const char *port_count() const		{ return "1/1-2"; }
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

  class SR2Forwarder *_sr_forwarder;

};


CLICK_ENDDECLS
#endif

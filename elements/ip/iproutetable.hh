#ifndef IPROUTETABLE_HH
#define IPROUTETABLE_HH

/*
 * =c
 * IPRouteTable
 * =s IP, classification
 * ip routing table super class
 * =d
 *
 * IPRouteTable defines the interface each IP routing table lookup
 * element must implement.
 *
 * IPRouteTable expects IP packets with dest IP addr annotations. for
 * each packet, it looks up the dest IP addr annotation in the routing
 * table, replaces the dest IP addr annotation with the new gateway,
 * and pushes the packet out on one of the outputs.
 *
 * Subclasses of IPRouteTable needs to implement four routines:
 * add_route, remove_route, lookup_route, and dump_routes. Replacing
 * annotation and pushing packets around are all taken care of by
 * IPRouteTable. the signatures for the routines that need to be
 * written are:
 *
 * void add_route(IPAddress dst, IPAddress mask, IPAddress gw, int port);
 * void remove_route(IPAddress dst, IPAddress mask);
 * int lookup_route(IPAddress dst, IPAddress &gw);  // returns port
 * String dump_routes();
 *
 * =h ctrl write
 * Take in changes to the routing table, in the format of 
 *
 *    add ip/mask [gw] output
 *    remove ip/mask
 *
 * for example,
 *
 *    add 18.26.4.0/24 18.26.4.1 0
 *
 * says all packets to 18.26.4.0/24 subnet should use gateway
 * 18.26.4.1, and go out on output port 0. and
 *
 *    remove 18.26.4.0/24
 *
 * removes the route.
 *
 * =h look read-only
 * Returns the contents of the routing table.
 *
 * =a LookupIPRoute, RadixIPLookup
 */

#include <click/glue.hh>
#include <click/element.hh>

class IPRouteTable : public Element {
public:
  IPRouteTable();
  ~IPRouteTable();
  
  const char *class_name() const	{ return "IPRouteTable"; }
  const char *processing() const	{ return PUSH; }
  virtual int initialize(ErrorHandler *){ return 0; }

  virtual IPRouteTable *clone() const	{ return new IPRouteTable; }
  virtual int configure(const Vector<String> &, ErrorHandler *)
    					{ return 0; }
  virtual String dump_routes()		{ return ""; }
  virtual void add_route(IPAddress, IPAddress, IPAddress, int) {}
  virtual void remove_route(IPAddress, IPAddress) {}
  virtual int lookup_route(IPAddress, IPAddress &) { return -1; }
  virtual void uninitialize()		{}
  
  void push(int port, Packet *p);
  static int ctrl_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static String look_handler(Element *, void *);
  void add_handlers();
};

#endif



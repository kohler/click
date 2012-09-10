#ifndef GRIDPROBESENDER_HH
#define GRIDPROBESENDER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * GridProbeSender(ETH, IP)
 * =s Grid
 * Produces a Grid route probe packet.
 * =d
 *
 * ETH and IP are this node's ethernet and IP addresses, respectively.
 * When the element's send_probe handler is called, pushes a
 * GRID_ROUTE_PROBE packet for the specified destination, with the
 * specified nonce.  This packet should probably be sent back through
 * the Grid input packet rocessing.
 *
 * =h send_probe write-only
 * arguments are the destination IP followed by a nonce, e.g: ``18.26.7.111 3242435''
 *
 *
 * =a GridProbeReplyReceiver, GridProbeHandler, LookupLocalGridRoute */

class GridProbeSender : public Element {

 public:
  GridProbeSender() CLICK_COLD;
  ~GridProbeSender() CLICK_COLD;

  const char *class_name() const		{ return "GridProbeSender"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void send_probe(IPAddress &, unsigned int);

  void add_handlers() CLICK_COLD;

private:
  IPAddress _ip;
  EtherAddress _eth;
};

CLICK_ENDDECLS
#endif

#ifndef GRIDGATEWAYINFO_HH
#define GRIDGATEWAYINFO_HH
#include <click/element.hh>
#include "grid.hh"
#include "gridgenericrt.hh"
CLICK_DECLS

/*
 * =c
 * GridGatewayInfo(DSDVRouteTable IS_GATEWAY)
 * =s Grid
 * =io
 * None
 * =d
 *
 * IS_GATEWAY is a boolean representing whether or not this node
 * should advertise itself as a gateway.
 *
 * This exists as a separate element for the sake of future
 * improvement.
 *
 * =h is_gateway read/write
 * Returns or sets boolean value of whether or not this node is a
 * gateway. */

class GridGatewayInfo : public Element {
  
public:

  GridGatewayInfo();
  ~GridGatewayInfo();

  const char *class_name() const { return "GridGatewayInfo"; }
  const char *processing() const		{ return AGNOSTIC; }
  GridGatewayInfo *clone() const { return new GridGatewayInfo; } // ?

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  void add_handlers();

  bool is_gateway ();
  static String print_best_gateway(Element *f, void *);

  Packet *simple_action(Packet *);
  class GridGenericRouteTable *_rt;
protected:

  bool _is_gateway;

};

CLICK_ENDDECLS
#endif

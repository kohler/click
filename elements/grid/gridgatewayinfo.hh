#ifndef GRIDGATEWAYINFO_HH
#define GRIDGATEWAYINFO_HH
#include <click/element.hh>
#include "grid.hh"
#include "gridgenericrt.hh"
CLICK_DECLS

/*
 * =c
 * GridGatewayInfo(GridGenericRouteTable, IS_GATEWAY)
 * =s Grid
 * Manage grid node gateway info. 
 *
 * =d
 *
 * GridGatewayInfo performs two functions [probably indicating a bad
 * design!]: first, it determines whether this particular node is a
 * gateway (IS_GATEWAY argument); second, it sets the destinination IP
 * address annotation of incoming packets to be the best current
 * gateway known by the node's routing table (GridGenericRouteTable
 * argument).
 *
 * GridGenericRouteTable is this node's route table.
 *
 * IS_GATEWAY is a boolean representing whether or not this node
 * should advertise itself as a gateway.
 *
 *
 * =h is_gateway read/write
 * Returns or sets boolean value of whether or not this node is a
 * gateway. 
 *
 * =a DSDVRouteTable, SetIPAddress */

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

#ifndef GRIDGATEWAYINFO_HH
#define GRIDGATEWAYINFO_HH
#include <click/element.hh>
#include "grid.hh"
CLICK_DECLS

/*
 * =c
 * GridGatewayInfo(IS_GATEWAY)
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

  GridGatewayInfo *clone() const { return new GridGatewayInfo; } // ?
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  void set_new_dest(double v_lat, double v_lon);

  bool is_gateway ();

  unsigned int _seq_no;

protected:

  bool _is_gateway;

};

CLICK_ENDDECLS
#endif

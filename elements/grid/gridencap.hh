#ifndef CLICK_GRIDENCAP_HH
#define CLICK_GRIDENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <elements/grid/grid.hh>
CLICK_DECLS

/*
 * =c
 *
 * GridEncap(ETH, IP)
 *
 * =s Grid
 *
 * Encapsulates packets in static Grid data encapsulation header
 * (GRID_NBR_ENCAP), including ethernet, Grid, and grid data encap
 * headers.
 *
 * =d
 *
 * ETH and IP are this node's ethernet and IP
 * addresses, respectively.  Sets the ethernet type to the Grid
 * ethertype; sets the ethernet source address to ETH.  Sets the Grid
 * src and tx IP to be IP.  Sets all locations to invalid.
 * Does not set checksum.
 *
 * Sets the grid destination IP to the packet's dest_ip annotation.
 *
 *
 * =a LookupLocalGridRoute, FixSrcLoc, FixDstLoc, SetGridChecksum */

class GridEncap : public Element { public:

  GridEncap() CLICK_COLD;
  ~GridEncap() CLICK_COLD;

  const char *class_name() const		{ return "GridEncap"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  EtherAddress _eth;
  IPAddress _ip;
  click_ether _eh;
  grid_hdr _gh;
  grid_nbr_encap _nb;
};

CLICK_ENDDECLS
#endif

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
 * GridEncap(MAC-ADDRESS, IP-ADDRESS)
 * 
 * =s Grid
 * 
 * Encapsulates packets in static Grid data encapsulation header
 * (GRID_NBR_ENCAP), including ethernet, Grid, and grid data encap
 * headers.
 *
 *  =d
 *
 * MAC-ADDRESS and IP-ADDRESS are this node's ethernet and IP
 * addresses, respectively.  Sets the ethernet type to the Grid
 * ethertype; sets the ethernet dest to MAC-ADDRESS.  Sets the Grid
 * src and tx IP to be IP-ADDRESS.  Sets all locations to invalid.
 * Does not set checksum.
 * 
 * Sets the grid destination IP to the packet's dest_ip annotation.
 *
 * 
 * =a LookupLocalGridRoute, FixSrcLoc, FixDstLoc, SetGridChecksum */ 

class GridEncap : public Element { public:
  
  GridEncap();
  ~GridEncap();
  
  const char *class_name() const		{ return "GridEncap"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  GridEncap *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  int initialize(ErrorHandler *);
  void add_handlers();

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

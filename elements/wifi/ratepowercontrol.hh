#ifndef CLICK_RATEPOWERCONTROL_HH
#define CLICK_RATEPOWERCONTROL_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
 * =c
 * 
 * RatePowerControl(MAC-ADDRESS, IP-ADDRESS)
 * 
 * =s Grid
 * 
 * Encapsulates packets in static Grid data encapsulation header
 * (GRID_NBR_ENCAP), including ethernet, Grid, and grid data encap
 * headers.
 *
 * =d
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

class RatePowerControl : public Element { public:
  
  RatePowerControl();
  ~RatePowerControl();
  
  const char *class_name() const		{ return "RatePowerControl"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);
  
 private:
  
  EtherAddress _eth;
};

CLICK_ENDDECLS
#endif

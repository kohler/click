#ifndef GEOROUTE_HH
#define GEOROUTE_HH

/*
 * =c
 * LookupGeographicGridRoute(MAC-ADDRESS, IP-ADDRESS, UpdateGridRoutes)
 *
 * =d 
 *
 * Forward packets geographically according to the tables accumulated by the
 * UpdateGridRoutes element.  MAC-ADDRESS and IP-ADDRESS are the local
 * machine's addresses.
 *
 * Input 0 expects GRID_NBR_ENCAP packets with MAC headers.  Assumes
 * packets aren't for us.
 *
 * Output 0 pushes out packets to be sent by a device, with next hop
 * info filled in.  These will be GRID_NBR_ENCAP packets with MAC
 * headers.
 *
 * Output 1 pushes out packets the LookupGeographicGridRoute doesn't
 * know how to handle (e.g. we don't know how to route around a hole).
 * These will also be GRID_NBR_ENCAP packets with MAC headers.
 *
 * Output 2 it the error output; it pushes out packets that are bad:
 * e.g., Grid protocol packets with an unknown type.
 *
 * =a
 * LookupLocalGridRoute
 * UpdateGridRoutes */

#include <click/element.hh>
#include <click/glue.hh>
#include "updateroutes.hh"
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>

class LookupGeographicGridRoute : public Element {
  public:

  LookupGeographicGridRoute();
  ~LookupGeographicGridRoute();

  const char *class_name() const		{ return "LookupGeographicGridRoute"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH; }
  LookupGeographicGridRoute *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void run_scheduled();

  void push(int port, Packet *);

private:

  bool get_next_geographic_hop(IPAddress dest_ip, grid_location dest_loc, EtherAddress *dest_eth) const;

  UpdateGridRoutes *_rt;
  EtherAddress _ethaddr;
  IPAddress _ipaddr;
};


#endif



#ifndef LOCALROUTE_HH
#define LOCALROUTE_HH

/*
 * =c
 * LookupLocalGridRoute(MAC-ADDRESS, IP-ADDRESS, UpdateGridRoutes)
 *
 * =d 
 * Forward packets according to the tables accumulated by the
 * UpdateGridRoutes element.  MAC-ADDRESS and IP-ADDRESS are the local
 * machine's addresses.
 *
 * Input 0 is from the device, output 0 is to the device.  Both should
 * be GRID_NBR_ENCAP packets with MAC headers.
 *
 * Input 1 is down from higher level protocols, output 1 is is up to
 * higher level protocols. The format of both is IP packets.
 *
 * Output 2 pushes out packets that LookupLocalGridRoute doesn't know what to do
 * with (we don't know have a route with a next hop), but are still
 * valid.  e.g. to be sent to geographic forwarding.
 *
 * Output 3 pushes out packets that are bad: e.g., Grid protocol
 * packets with an unknown type, or too many hops have been
 * travelled.  
 *
 * =a
 * UpdateGridRoutes */

#include "element.hh"
#include "glue.hh"
#include "updateroutes.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"

class LookupLocalGridRoute : public Element {
  public:

  LookupLocalGridRoute();
  ~LookupLocalGridRoute();

  const char *class_name() const		{ return "LookupLocalGridRoute"; }
  void *cast(const char *);
  const char *processing() const		{ return PUSH; }
  LookupLocalGridRoute *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void run_scheduled();

  void push(int port, Packet *);

private:

  void forward_grid_packet(Packet *packet, IPAddress dest_ip);

  UpdateGridRoutes *_nbr;
  int _max_forwarding_hops;
  EtherAddress _ethaddr;
  IPAddress _ipaddr;
};


#endif

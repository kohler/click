#ifndef LOCALROUTE_HH
#define LOCALROUTE_HH

/*
 * LocalRoute(mac-address, ip-address, neighbor-element)
 *
 * Forward packets according to the tables accumulated
 * by Neighbor.
 *
 * Input 0 is from the device, output 0 is to the device.  Both should
 * be GRID_NBR_ENCAP packets with MAC headers.
 *
 * Input 1 is down from higher level protocols, output 1 is is up to
 * higher level protocols. The format of both is IP packets.
 *
 * Output 2 is where all the packets go the LocalRoute doesn't know
 * what to do with.  e.g., Grid protocol packets with an unknown type,
 * too many hops have been travelled, or there is no next hop.  */

#include "element.hh"
#include "glue.hh"
#include "neighbor.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"

class LocalRoute : public Element {
  public:

  LocalRoute();
  ~LocalRoute();

  const char *class_name() const		{ return "LocalRoute"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  LocalRoute *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void run_scheduled();

  void push(int port, Packet *);

private:

  void forward_grid_packet(Packet *packet, IPAddress dest_ip);

  Neighbor *_nbr;
  int _max_forwarding_hops;
  EtherAddress _ethaddr;
  IPAddress _ipaddr;
};


#endif

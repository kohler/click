#ifndef LOCALROUTE_HH
#define LOCALROUTE_HH

/*
 * Run the Grid multihop local routing protocol.
 */

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

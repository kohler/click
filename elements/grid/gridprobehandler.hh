#ifndef GRIDPROBEHANDLER_HH
#define GRIDPROBEHANDLER_HH

/*
 * =c
 * GridProbeHandler(E, I, LookupLocalGridRoute, LookupGeographicGridRoute)
 * =s Grid
 * Handles Grid route probes, producing probe replies
 * =d
 * 
 * E and I are this nodes's ethernet and IP addresses, respectively.
 * The 3rd and 4th arguments are the configuration's local and
 * geographic forwarding elements.  They are required so that the
 * probe replies can contain information about the routing actions
 * taken for this packet.
 *
 * When a Grid probe is received on its input, pushes a probe reply
 * out of its second output, and pushes the probe out of the first
 * output (if the probe should be forwarded).
 *
 * =a GridProbeSender, GridProbeReplyReceiver, LookupLocalGridRoute,
 * LookupGeographicGridroute */


#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>
#include "gridroutecb.hh"
#include "lookuplocalgridroute.hh"
#include "lookupgeogridroute.hh"

class GridProbeHandler : public Element, GridRouteActionCallback {

 public:
  GridProbeHandler();
  ~GridProbeHandler();
  
  const char *class_name() const		{ return "GridProbeHandler"; }
  const char *processing() const		{ return PUSH; }
  GridProbeHandler *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int, Packet *);

  void route_cb(int id, unsigned int dest_ip, Action a, unsigned int data, unsigned int data2);

private:
  EtherAddress _eth;
  IPAddress _ip;

  int _gf_cb_id;
  int _lr_cb_id;

  LookupLocalGridRoute *_lr_el;
  LookupGeographicGridRoute *_gf_el;

  Packet *_cached_reply_pkt;
};

#endif

#ifndef GRIDPROBEHANDLER_HH
#define GRIDPROBEHANDLER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>
#include "gridroutecb.hh"
#include "lookuplocalgridroute.hh"
#include "lookupgeogridroute.hh"
#include "floodinglocquerier.hh"
CLICK_DECLS

/*
 * =c
 * GridProbeHandler(ETH, IP, LookupLocalGridRoute, LookupGeographicGridRoute, FloodingLocQuerier)
 * =s Grid
 * Handles Grid route probes, producing probe replies
 * =d
 *
 * ETH and IP are this nodes's ethernet and IP addresses, respectively.
 * The 3rd, 4th, and 5th arguments are the configuration's local and
 * geographic forwarding elements.  They are required so that the
 * probe replies can contain information about the routing actions
 * taken for this packet.
 *
 * When a Grid probe is received on its input, pushes a probe reply
 * out of its second output, and pushes the probe out of the first
 * output (if the probe should be forwarded).
 *
 * =a GridProbeSender, GridProbeReplyReceiver, LookupLocalGridRoute,
 * LookupGeographicGridroute, FloodingLocQuerier */

class GridProbeHandler : public Element, GridRouteActionCallback {

 public:
  GridProbeHandler() CLICK_COLD;
  ~GridProbeHandler() CLICK_COLD;

  const char *class_name() const		{ return "GridProbeHandler"; }
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);

  void route_cb(int id, unsigned int dest_ip, Action a, unsigned int data, unsigned int data2);

private:
  EtherAddress _eth;
  IPAddress _ip;

  int _gf_cb_id;
  int _fq_cb_id;
  int _lr_cb_id;

  LookupLocalGridRoute *_lr_el;
  LookupGeographicGridRoute *_gf_el;
  FloodingLocQuerier *_fq_el;

  Packet *_cached_reply_pkt;
};

CLICK_ENDDECLS
#endif

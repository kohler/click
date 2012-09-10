#ifndef GEOROUTE_HH
#define GEOROUTE_HH

/*
 * =c
 * LookupGeographicGridRoute(ETH, IP, GRIDROUTES, LOCINFO)
 *
 * =s Grid
 * =d
 *
 * Forward packets geographically according to the tables accumulated
 * by the UpdateGridRoutes element, and the node's own position as
 * reported by the GridLocationInfo element.  MAC-ADDRESS and
 * IP-ADDRESS are the local machine's addresses.
 *
 * Input 0 expects Grid packets with MAC headers.  Assumes
 * packets aren't for us.
 *
 * Output 0 pushes out packets to be sent by a device, with next hop
 * info filled in.  These will be Grid packets with MAC headers.
 * Output packets have their paint annotation set to the output
 * interface number, e.g. for use with PaintSwitch.
 *
 * Output 1 is the error output: it pushes out packets the
 * LookupGeographicGridRoute doesn't know how to handle (e.g. we don't
 * know how to route around a hole, or we don't know our own
 * location), or are bad (e.g. some type we don't know how to handle).
 * These will also be Grid packets with MAC headers.
 *
 * =a
 * LookupLocalGridRoute
 * GridGenericRouteTable
 * GridLocationInfo
 * Paint
 * PaintSwitch
 */

#include <click/element.hh>
#include <click/glue.hh>
#include "grid.hh"
#include "gridroutecb.hh"
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/task.hh>
CLICK_DECLS

class GridGenericRouteTable;
class GridLocationInfo;

class LookupGeographicGridRoute : public Element, public GridRouteActor {
  public:

  LookupGeographicGridRoute() CLICK_COLD;
  ~LookupGeographicGridRoute() CLICK_COLD;

  const char *class_name() const		{ return "LookupGeographicGridRoute"; }
  void *cast(const char *);
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  bool run_task(Task *);

  void push(int port, Packet *);

private:

  bool get_next_geographic_hop(grid_location dest_loc, EtherAddress *dest_eth,
			       IPAddress *dest_ip, IPAddress *best_nbr_ip,
			       unsigned char *next_hop_interface) const;

  void increment_hops_travelled(WritablePacket *p) const;
  bool dont_forward(const Packet *p) const;
  bool dest_loc_good(const Packet *p) const;
  grid_location get_dest_loc(const Packet *p) const;

  GridGenericRouteTable *_rt;
  GridLocationInfo *_li;
  EtherAddress _ethaddr;
  IPAddress _ipaddr;
  Task _task;
};

CLICK_ENDDECLS
#endif

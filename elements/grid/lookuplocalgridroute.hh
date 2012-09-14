#ifndef LOCALROUTE_HH
#define LOCALROUTE_HH

/*
 * =c
 * LookupLocalGridRoute(ETH, IP, GenericGridRouteTable, I<KEYWORDS>)
 *
 * =s Grid
 * =d
 * Forward packets according to the tables accumulated by the
 * GenericGridRouteTable element.  MAC-ADDRESS and IP-ADDRESS are the local
 * machine's addresses.
 *
 * Input 0 is from the device, output 0 is to the device.  Both should
 * be GRID_NBR_ENCAP packets with MAC headers.  Output packets have
 * their paint annotation set to the output interface number, e.g. for
 * use with PaintSwitch.
 *
 * Input 1 is down from higher level protocols, output 1 is is up to
 * higher level protocols. The format of both is IP packets.
 *
 * Output 2 pushes out packets that LookupLocalGridRoute doesn't know
 * what to do with (we don't know have a route with a next hop), but
 * are still valid.  e.g. to be sent to geographic forwarding.  These
 * packets are GRID_NBR_ENCAP packets with MAC headers.
 *
 * Output 3 is the error output; it pushes out packets that are bad:
 * e.g., Grid protocol packets with an unknown type.
 *
 * Keywords are:
 *
 * =over 8
 *
 * =item GWI
 *
 * GridGatewayInfo element.  If provided, and indicates this node is a
 * gateway, send incoming packets for the generic gateway address to
 * output 1.
 *
 * =item LOG
 *
 * GridGenericLogger element.  Object to log events to.
 *
 * =item LT
 *
 * LinkTracker element.  If provided, used to obtain link stats for
 * route callbacks.  Confused?  Me too!
 *
 * =back
 *
 * =a
 * LookupGeographicGridRoute, GenericGridRouteTable,
 * GridGatewayInfo, LinkTracker, GridLogger, Paint, PaintSwitch */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/task.hh>

#include <elements/grid/gridroutecb.hh>
CLICK_DECLS

class GridGatewayInfo;
class LinkTracker;
class GridGenericLogger;
class GridGenericRouteTable;

class LookupLocalGridRoute : public Element, public GridRouteActor  {
  public:

  LookupLocalGridRoute() CLICK_COLD;
  ~LookupLocalGridRoute() CLICK_COLD;

  const char *class_name() const		{ return "LookupLocalGridRoute"; }
  void *cast(const char *);
  const char *port_count() const		{ return "2/4"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  bool run_task(Task *);

  void push(int port, Packet *);

private:

  bool get_next_hop(IPAddress dest_ip, EtherAddress *dest_eth,
		    IPAddress *next_hop_ip, unsigned char *next_hop_interface) const;
  void forward_grid_packet(Packet *packet, IPAddress dest_ip);

  GridGatewayInfo *_gw_info;
  LinkTracker *_link_tracker;
  GridGenericRouteTable *_rtes;
  EtherAddress _ethaddr;
  IPAddress _ipaddr;
  IPAddress _any_gateway_ip;
  Task _task;

  GridGenericLogger *_log;

  bool is_gw();
};

CLICK_ENDDECLS
#endif

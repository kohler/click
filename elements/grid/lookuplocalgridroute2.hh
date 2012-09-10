#ifndef LOCALROUTE2_HH
#define LOCALROUTE2_HH

/*
 * =c
 * LookupLocalGridRoute2(ETH, IP, GenericGridRouteTable, I<KEYWORDS>)
 *
 * =s Grid
 *
 * =d
 *
 * Forward packets according to the tables accumulated by the
 * GenericGridRouteTable element.
 *
 * ETH and IP are this node's ethernet and IP addresses, respectively.
 *
 * Inputs must be GRID_NBR_ENCAP packets with MAC headers, with the
 * destination IP address annotation set.  Output packets have their
 * source ethernet address set to ETH, their destination ethernet
 * address set according to the routing table entry corresponding to
 * the destination IP annotation, and their Grid tx_ip set to IP.
 * Packets also have their paint annotation set to the output
 * interface number, e.g. for use with PaintSwitch.
 *
 * Packets for which no route exists are dropped.
 *
 * Keywords are:
 *
 * =over 8
 *
 * =item LOG
 *
 * GridGenericLogger element.  Object to log events to.
 *
 * =item VERBOSE
 *
 * Boolean.  Be verbose about drops due to no route.  Defaults to false.
 *
 * =back
 *
 * =a LookupLocalGridroute, LookupGeographicGridRoute,
 * GenericGridRouteTable, DSDVRouteTable, GridLogger, Paint,
 * PaintSwitch */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/task.hh>
#include <elements/grid/gridroutecb.hh>
CLICK_DECLS

class GridGenericLogger;
class GridGenericRouteTable;

class LookupLocalGridRoute2 : public Element, public GridRouteActor  {

public:

  LookupLocalGridRoute2();
  ~LookupLocalGridRoute2();

  const char *class_name() const	{ return "LookupLocalGridRoute2"; }
  void *cast(const char *);
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:

  Packet *forward_grid_packet(Packet *packet, IPAddress dest_ip);

  GridGenericRouteTable *_rtes;
  GridGenericLogger *_log;
  EtherAddress _eth;
  IPAddress _ip;
  bool _verbose;
};

CLICK_ENDDECLS
#endif

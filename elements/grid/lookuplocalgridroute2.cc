/*
 * lookuplocalgridroute2.{cc,hh} -- Agnostic Grid multihop local routing element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/glue.hh>

#include <elements/grid/grid.hh>
#include <elements/grid/lookuplocalgridroute2.hh>
#include <elements/grid/gridgenericrt.hh>
#include <elements/grid/gridgenericlogger.hh>

CLICK_DECLS

LookupLocalGridRoute2::LookupLocalGridRoute2()
  : _rtes(0), _log(0), _verbose(false)
{
}

LookupLocalGridRoute2::~LookupLocalGridRoute2()
{
}

void *
LookupLocalGridRoute2::cast(const char *n)
{
  if (strcmp(n, "LookupLocalGridRoute2") == 0)
    return (LookupLocalGridRoute2 *)this;
  else
    return 0;
}

int
LookupLocalGridRoute2::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ETH", _eth)
	.read_mp("IP", _ip)
	.read_p("GRIDROUTES", reinterpret_cast<Element *&>(_rtes))
	.read("LOG", reinterpret_cast<Element *&>(_log))
	.read("VERBOSE", _verbose)
	.complete();
}

int
LookupLocalGridRoute2::initialize(ErrorHandler *errh)
{

  if (_rtes && _rtes->cast("GridGenericRouteTable") == 0) {
    return errh->error("%s: GridRouteTable argument %s has the wrong type",
		       name().c_str(),
		       _rtes->name().c_str());
  }
#if 0
  else if (_rtes == 0) {
    return errh->error("%s: no GridRouteTable element given",
		       name().c_str());
  }
#endif

  if (_log && _log->cast("GridGenericLogger") == 0) {
    return errh->error("%s: GridGenericLogger element %s has the wrong type",
		       name().c_str(),
		       _log->name().c_str());
  }
  return 0;
}

typedef GridRouteActionCallback GRCB;

Packet *
LookupLocalGridRoute2::simple_action(Packet *packet)
{
  assert(packet);
  if (packet->length() < sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap)) {
    click_chatter("LookupLocalGridRoute2 %s: packet too small (%d), dropping",
		  name().c_str(), packet->length());
    notify_route_cbs(packet, packet->dst_ip_anno(), GRCB::Drop, GRCB::BadPacket, 0);
    packet->kill();
    return 0;
  }

  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY:
      return forward_grid_packet(packet, packet->dst_ip_anno());
      break;
  default:
    click_chatter("LookupLocalGridRoute2 %s: received unexpected Grid packet type (%s), dropping",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
    notify_route_cbs(packet, packet->dst_ip_anno(), GRCB::Drop, GRCB::UnknownType, 0);
    packet->kill();
    return 0;
  }
}


Packet *
LookupLocalGridRoute2::forward_grid_packet(Packet *xp, IPAddress dest_ip)
{
  WritablePacket *packet = xp->uniqueify();

  if (_rtes == 0) {
    click_chatter("LookupLocalGridRoute2 %s: there is no routing table, dropping", name().c_str());
    notify_route_cbs(packet, dest_ip, GRCB::Drop, GRCB::ConfigError, 0);
    packet->kill();
    return 0;
  }

  GridGenericRouteTable::RouteEntry rte;
  if (_rtes->get_one_entry(dest_ip, rte)) {

    struct click_ether *eh = (click_ether *) packet->data();
    memcpy(eh->ether_dhost, rte.next_hop_eth.data(), 6);
    memcpy(eh->ether_shost, _eth.data(), 6);

    struct grid_hdr *gh = (grid_hdr *) (eh + 1);
    gh->tx_ip = _ip.addr();

    struct grid_nbr_encap *encap = (grid_nbr_encap *) (gh + 1);
    encap->hops_travelled++;

    SET_PAINT_ANNO(packet, rte.next_hop_interface);
    return packet;
  }
  else {
    // logging

    if (_verbose)
      click_chatter("LookupLocalGridRoute2 %s: no route to %s, dropping", name().c_str(), dest_ip.unparse().c_str());

    notify_route_cbs(packet, dest_ip, GRCB::Drop, GRCB::NoLocalRoute, 0);

    if (_log)
      _log->log_no_route(packet, Timestamp::now());

    packet->kill();
    return 0;
  }
}

CLICK_ENDDECLS

EXPORT_ELEMENT(LookupLocalGridRoute2)

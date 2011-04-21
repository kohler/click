/*
 * lookupgeogridroute.{cc,hh} -- Grid geographic routing element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <stddef.h>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/glue.hh>
#include "grid.hh"
#include "lookupgeogridroute.hh"
#include "gridgenericrt.hh"
#include "filterbyrange.hh"
#include "gridlocationinfo.hh"

CLICK_DECLS

LookupGeographicGridRoute::LookupGeographicGridRoute()
  : _rt(0), _task(this)
{
}

LookupGeographicGridRoute::~LookupGeographicGridRoute()
{
}

void *
LookupGeographicGridRoute::cast(const char *n)
{
  if (strcmp(n, "LookupGeographicGridRoute") == 0)
    return (LookupGeographicGridRoute *)this;
  else
    return 0;
}

int
LookupGeographicGridRoute::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ETH", _ethaddr)
	.read_mp("IP", _ipaddr)
	.read_mp("GRIDROUTES", ElementCastArg("GridGenericRouteTable"), _rt)
	.read_mp("LOCINFO", ElementCastArg("GridLocationInfo"), _li)
	.complete();
}

int
LookupGeographicGridRoute::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

bool
LookupGeographicGridRoute::run_task(Task *)
{
  Packet *p = input(0).pull();
  if (p)
    push(0, p);
  _task.fast_reschedule();
  return p != 0;
}

typedef GridRouteActionCallback GRCB;

void
LookupGeographicGridRoute::push(int port, Packet *packet)
{
  /*
   * expects packets with MAC header and Grid NBR_ENCAP header
   */
  assert(packet);
  assert(port == 0);

  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));

  if (dont_forward(packet)) {
    click_chatter("LookupGeographicGridRoute %s: not supposed to forward packet of type %s",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
    notify_route_cbs(packet, 0, GRCB::Drop, GRCB::UnknownType, 0);
    output(1).push(packet);
    return;
  }

  unsigned dest_ip = 0; // XXX this is a total fuckup, since we can only extract dest_ip for nbr_encap packets
  if (gh->type == grid_hdr::GRID_NBR_ENCAP) {
    struct grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);
    dest_ip = nb->dst_ip;
  }


  if (!_li->loc_good()) {
    click_chatter("LookupGeographicGridRoute %s: can't forward packet; we don't know our own location", name().c_str());
    notify_route_cbs(packet, dest_ip, GRCB::Drop, GRCB::OwnLocUnknown, 0);
    output(1).push(packet);
    return;
  }

  if (!dest_loc_good(packet)) {
    click_chatter("LookupGeographicGridRoute %s: received packet of type %s without good destination location",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
    notify_route_cbs(packet, dest_ip, GRCB::Drop, GRCB::NoDestLoc, 0);
    output(1).push(packet);
    return;
  }


  EtherAddress next_hop_eth;
  IPAddress next_hop_ip;
  unsigned char next_hop_interface = 0;
  IPAddress best_nbr_ip;
  bool found_next_hop = get_next_geographic_hop(get_dest_loc(packet), &next_hop_eth, &next_hop_ip,
						&best_nbr_ip, &next_hop_interface);

  WritablePacket *xp = packet->uniqueify();

  if (found_next_hop) {
    struct click_ether *eh = (click_ether *) xp->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth.data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (xp->data() + sizeof(click_ether));
    gh->tx_ip = _ipaddr;
    increment_hops_travelled(xp);
    notify_route_cbs(packet, dest_ip, GRCB::ForwardGF, next_hop_ip, best_nbr_ip);
    // leave src location update to FixSrcLoc element
    SET_PAINT_ANNO(xp, next_hop_interface);
    output(0).push(xp);
  }
  else {
    click_chatter("LookupGeographicGridRoute %s: unable to forward %s packet with geographic routing",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
#if 0
    if (gh->type ==  grid_hdr::GRID_NBR_ENCAP) {
      int ip_off = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap);
      xp->pull(ip_off);
      IPAddress src_ip(xp->data() + 12);
      IPAddress dst_ip(xp->data() + 16);
      unsigned short *sp = (unsigned short *) (xp->data() + 20);
      unsigned short *dp = (unsigned short *) (xp->data() + 22);
      unsigned short src_port = ntohs(*sp);
      unsigned short dst_port = ntohs(*dp);
      click_chatter("IP packet info: %s:%hu -> %s:%hu", src_ip.unparse().c_str(), src_port, dst_ip.s().c_str(), dst_port);
    }
#endif
    notify_route_cbs(packet, dest_ip, GRCB::Drop, GRCB::NoCloserNode, 0);
    output(1).push(xp);
  }
}


bool
LookupGeographicGridRoute::get_next_geographic_hop(grid_location dest_loc, EtherAddress *dest_eth,
						   IPAddress *dest_ip, IPAddress *best_nbr,
						   unsigned char *next_hop_interface) const
{
  /*
   * search through table for all nodes we have routes to and for whom
   * we know the position.  of these, choose the node closest to the
   * destination location to send the packet to.  Our node may be the
   * closest; in that case, the packet should be dropped.
   */

  assert(_li->loc_good());
  double d = grid_location::calc_range(dest_loc, _li->get_current_location());
  bool found_one = false;

  Vector<GridGenericRouteTable::RouteEntry> rtes;
  _rt->get_all_entries(rtes);

  GridGenericRouteTable::RouteEntry best_nbr_entry;

  for (int i = 0; i < rtes.size(); i++) {
    const GridGenericRouteTable::RouteEntry &rte = rtes[i];
    if (!rte.loc_good)
      continue;
    double new_d = grid_location::calc_range(dest_loc, rte.dest_loc);
    if (!found_one) {
      found_one = true;
      d = new_d;
      best_nbr_entry = rte;
    }
    else if (new_d < d) {
      d = new_d;
      best_nbr_entry = rte;
    }
  }

  if (!found_one)
    return false;

  /* XXX fooey, we may actually send the packet backwards here even
     though we choose a next hop to some node which is closest to
     ultimate dest.  the issues is: we can't mark the packet with some
     intermediate destination address, only the next hop and ultimate
     destination... how to `fix' the phase of the packet so we can
     make progree guarantees? -- Now I think this is okay, assuming
     the node movement time-scale is much greater than than the packet
     time-of-flight.  Basically, the DSDV tables will be consistent
     across hops, so that no intermediate forwarding node will make a
     backwards decision. -- decouto  */

  *dest_eth = best_nbr_entry.next_hop_eth;
  *dest_ip = best_nbr_entry.next_hop_ip;
  *next_hop_interface = best_nbr_entry.next_hop_interface;
  *best_nbr = best_nbr_entry.dest_ip;

  return true;
}


void
LookupGeographicGridRoute::increment_hops_travelled(WritablePacket *p) const
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  struct grid_nbr_encap *encap = 0;
  struct grid_geocast *gc = 0;

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY:
    encap = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    encap->hops_travelled++;
    break;
  case grid_hdr::GRID_GEOCAST:
    gc = (grid_geocast *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    gc->hops_travelled++;
    break;
  default:
    /* packet doesn't have a hops_travelled field */
    break;
  }
}

bool
LookupGeographicGridRoute::dont_forward(const Packet *p) const
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  struct grid_nbr_encap *encap = 0;
  struct grid_geocast *gc = 0;

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY:
    encap = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    return IPAddress(encap->dst_ip) == _ipaddr;
    break;
  case grid_hdr::GRID_GEOCAST:
    gc = (grid_geocast *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    assert(_li->loc_good());
    return gc->dst_region.contains(_li->get_current_location());
    break;
  default:
    return true;
    break;
  }
  return true;
}

bool
LookupGeographicGridRoute::dest_loc_good(const Packet *p) const
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY: {
#ifndef SMALL_GRID_HEADERS
    struct grid_nbr_encap *encap = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    return encap->dst_loc_good;
#else
    return false;
#endif
    break;
  }
  case grid_hdr::GRID_GEOCAST:
    return true;
    break;
  default:
    return false;
    break;
  }
  return false;
}

grid_location
LookupGeographicGridRoute::get_dest_loc(const Packet *p) const
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY: {
#ifndef SMALL_GRID_HEADERS
    struct grid_nbr_encap *encap = (grid_nbr_encap *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    return encap->dst_loc;
#else
    return grid_location(0, 0, 0);
#endif
    break;
  }
  case grid_hdr::GRID_GEOCAST: {
    struct grid_geocast *gc = (grid_geocast *) (p->data() + sizeof(click_ether) + gh->hdr_len);
    return gc->dst_region.center();
    break;
  }
  default:
    assert(0);
    break;
  }
  return grid_location(0, 0, 0);
}


/* XXX I feel like there is a general pattern here of filling in the
 * packet based on some information looked up in the routing table.
 * could get all generic here and provide an interface to the table
 * for generic visitors to pull out desired info, and a generic
 * ``lookup-and-modify-packet'' element that lets me plug in the
 * appropriate visitors... i guess i could use the iterators... */

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LookupGeographicGridRoute)

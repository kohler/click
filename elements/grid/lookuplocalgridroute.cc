/*
 * lookuplocalgridroute.{cc,hh} -- Grid multihop local routing element
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
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/glue.hh>

#include <elements/grid/grid.hh>
#include <elements/grid/gridgatewayinfo.hh>
#include <elements/grid/lookuplocalgridroute.hh>
#include <elements/grid/linktracker.hh>
#include <elements/grid/gridgenericrt.hh>
#include <elements/grid/gridgenericlogger.hh>

CLICK_DECLS

int GridRouteActor::_next_free_cb = 0;

#define NOISY 0

LookupLocalGridRoute::LookupLocalGridRoute()
  : _gw_info(0), _link_tracker(0), _rtes(0),
  _any_gateway_ip(0), _task(this),
  _log(0)
{
}

LookupLocalGridRoute::~LookupLocalGridRoute()
{
}

void *
LookupLocalGridRoute::cast(const char *n)
{
  if (strcmp(n, "LookupLocalGridRoute") == 0)
    return (LookupLocalGridRoute *)this;
  else
    return 0;
}

int
LookupLocalGridRoute::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("ETH", _ethaddr)
      .read_mp("IP", _ipaddr)
      .read_mp("GRIDROUTES", reinterpret_cast<Element *&>(_rtes))
      .read("GWI", reinterpret_cast<Element *&>(_gw_info))
      .read("LT", reinterpret_cast<Element *&>(_link_tracker))
      .read("LOG", reinterpret_cast<Element *&>(_log))
      .complete();
  _any_gateway_ip = htonl((ntohl(_ipaddr.addr()) & 0xFFffFF00) | 254);
  return res;
}

int
LookupLocalGridRoute::initialize(ErrorHandler *errh)
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

  if (_gw_info && _gw_info->cast("GridGatewayInfo") == 0) {
    return errh->error("%s: GridGatewayInfo argument %s has the wrong type",
		       name().c_str(),
		       _gw_info->name().c_str());
  }

  if (_link_tracker && _link_tracker->cast("LinkTracker") == 0) {
    return errh->error("%s: LinkTracker argument %s has the wrong type",
		       name().c_str(),
		       _link_tracker->name().c_str());
  }

  if (_log && _log->cast("GridGenericLogger") == 0) {
    return errh->error("%s: GridGenericLogger element %s has the wrong type",
		       name().c_str(),
		       _log->name().c_str());
  }

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

bool
LookupLocalGridRoute::run_task(Task *)
{
  Packet *p = input(0).pull();
  if (p)
    push(0, p);
  _task.fast_reschedule();
  return p != 0;
}

bool
LookupLocalGridRoute::is_gw()
{
  return _gw_info && _gw_info->is_gateway();
}


typedef GridRouteActionCallback GRCB;

void
LookupLocalGridRoute::push(int port, Packet *packet)
{
  /*
   * input 1 and output 1 hook up to higher level (e.g. ip), input 0
   * and output 0 hook up to lower level (e.g. ethernet)
   */

  assert(packet);

  if (port == 0) {
    /*
     * input from net device
     */
    grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
    switch (gh->type) {

    case grid_hdr::GRID_NBR_ENCAP:
    case grid_hdr::GRID_LOC_REPLY:
    case grid_hdr::GRID_ROUTE_PROBE:
    case grid_hdr::GRID_ROUTE_REPLY:
      {
	/*
	* try to either receive the packet or forward it
	*/
	struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + gh->hdr_len);
	IPAddress dest_ip(encap->dst_ip);
#if NOISY
	click_chatter("lr %s: got %s packet for %s; I am %s; agi=%s, is_gw = %d\n",
		      name().c_str(),
		      grid_hdr::type_string(gh->type).c_str(),
		      dest_ip.unparse().c_str(),
		      _ipaddr.unparse().c_str(),
		      _any_gateway_ip.unparse().c_str(),
		      is_gw() ? 1 : 0);
#endif
	// is the packet for us?
	if ((dest_ip == _ipaddr) ||
	    (dest_ip == _any_gateway_ip && is_gw())) {
	  // is it IP data?  If so, send it to IP input path.
	  if (gh->type == grid_hdr::GRID_NBR_ENCAP) {
#if NOISY
	    click_chatter("%s: got an IP packet for us %s",
			  name().c_str(),
			  dest_ip.unparse().c_str());
#endif
	    packet->pull(sizeof(click_ether) + gh->hdr_len + sizeof(grid_nbr_encap));
	    notify_route_cbs(packet, dest_ip, GRCB::SendToIP, 0, 0);
	    output(1).push(packet);
	  }
	  else
	    click_chatter("%s: got %s packet for us, but don't know how to handle it",
			  name().c_str(), grid_hdr::type_string(gh->type).c_str());
	}
	else {
	  // packet is not for us, try to forward it!
	  forward_grid_packet(packet, encap->dst_ip);
	}
      }
      break;

    default:
      click_chatter("%s: received unexpected Grid packet type: %s",
		    name().c_str(), grid_hdr::type_string(gh->type).c_str());
      notify_route_cbs(packet, 0, GRCB::Drop, GRCB::UnknownType, 0);
      output(3).push(packet);
    }
  }
  else {
    /*
     * input from higher level protocol -- expects IP packets
     * annotated with dst IP address.
     */
    assert(port == 1);
    // check to see is the desired dest is our neighbor
    IPAddress dst = packet->dst_ip_anno();
#if NOISY
    click_chatter("lr %s: got packet for %s; I am %s; agi=%s, is_gw=%d\n",
		  name().c_str(),
		  dst.unparse().c_str(),
		  _ipaddr.unparse().c_str(),
		  _any_gateway_ip.unparse().c_str(),
		  is_gw() ? 1 : 0);
#endif
    if (dst == _any_gateway_ip && is_gw()) {
      packet->kill();
    } else if (dst == _ipaddr) {
      click_chatter("%s: got IP packet from us for our address; looping it back.  Check the configuration.", name().c_str());
      output(1).push(packet);
    } else {
      // encapsulate packet with grid hdr and try to send it out

      WritablePacket *new_packet = packet->push(sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap));
      memset(new_packet->data(), 0, sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap));

      struct click_ether *eh = (click_ether *) new_packet->data();
      eh->ether_type = htons(ETHERTYPE_GRID);

      struct grid_hdr *gh = (grid_hdr *) (new_packet->data() + sizeof(click_ether));
      gh->hdr_len = sizeof(grid_hdr);
      gh->total_len = new_packet->length() - sizeof(click_ether); // encapsulate everything we get, don't look inside it for length info
      gh->total_len = htons(gh->total_len);
      gh->type = grid_hdr::GRID_NBR_ENCAP;

      /* FixSrcLoc will see that the gh->ip and gh->tx_ip fields are
         the same, and will fill in gh->loc, etc. */
      // gh->tx_ip is set in forward_grid_packet()
      gh->ip = _ipaddr;

      struct grid_nbr_encap *encap = (grid_nbr_encap *) (new_packet->data() + sizeof(click_ether) + sizeof(grid_hdr));
      encap->hops_travelled = 0;
      encap->dst_ip = dst;

#ifndef SMALL_GRID_HEADERS
      encap->dst_loc_good = false;
#endif
      forward_grid_packet(new_packet, dst);
    }
  }
}

bool
LookupLocalGridRoute::get_next_hop(IPAddress dest_ip, EtherAddress *dest_eth,
				   IPAddress *next_hop_ip, unsigned char *next_hop_interface) const
{
  assert(dest_eth != 0);

  GridGenericRouteTable::RouteEntry rte;
  bool found_route;

  if (dest_ip == _any_gateway_ip) {

    found_route = _rtes->current_gateway(rte);

    // what if we are being asked to forward a grid packet to an
    // internet host, but we have no local routes to a gateway?
    // drop here?

    // i guess we could do a loc query for a gateway...  seems a little
    // sketchy to me though.

  } else {
    found_route = _rtes->get_one_entry(dest_ip, rte);
  }

  /* did we have a route? */
  if (found_route && rte.good()) {
    *dest_eth = rte.next_hop_eth;
    *next_hop_ip = rte.next_hop_ip;
    *next_hop_interface = rte.next_hop_interface;
    return true;
  }

  return false;
}


void
LookupLocalGridRoute::forward_grid_packet(Packet *xp, IPAddress dest_ip)
{
  WritablePacket *packet = xp->uniqueify();

  /*
   * packet must have a MAC hdr, grid_hdr, and a grid_nbr_encap hdr on
   * it.  This function will update the hop count, transmitter ip and
   * loc (us) and dst/src MAC addresses.  Sender ip and loc info will
   * not be touched, so if we are originating the packet, those have
   * to be setup before calling this function.  Similarly, the
   * destination ip and loc info must be set by whoever (or whatever
   * element) originates the packet.  The originator should probably
   * set the nb->dst_loc_good to false, so that if we don't find a
   * local route the loc querier will know to lookup the destination
   * location before using geographic forwarding.
   */

  if (_rtes == 0) {
    // no GridRouteTable next-hop table in configuration
    click_chatter("%s: can't forward packet for %s; there is no routing table, trying geographic forwarding", name().c_str(), dest_ip.unparse().c_str());
    notify_route_cbs(packet, dest_ip, GRCB::FallbackToGF, 0, 0);
    output(2).push(packet);
    return;
  }

  struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));

  EtherAddress next_hop_eth;
  IPAddress next_hop_ip;
  unsigned char next_hop_interface = 0;
  bool found_next_hop = get_next_hop(dest_ip, &next_hop_eth, &next_hop_ip, &next_hop_interface);

  if (found_next_hop) {
    struct click_ether *eh = (click_ether *) packet->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth.data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
    gh->tx_ip = _ipaddr;
    encap->hops_travelled++;
    // leave src location update to FixSrcLoc element
    int sig = 0;
    int qual = 0;
    Timestamp tv;
#ifdef CLICK_USERLEVEL
    if (_link_tracker)
      _link_tracker->get_stat(next_hop_ip, sig, qual, tv);
#endif
    unsigned int data2 = (qual << 16) | ((-sig) & 0xFFff);
    notify_route_cbs(packet, dest_ip, GRCB::ForwardDSDV, next_hop_ip, data2);
    SET_PAINT_ANNO(packet, next_hop_interface);
    output(0).push(packet);
  }
  else {
#if NOISY
    click_chatter("%s: unable to forward packet for %s with local routing, trying geographic routing", name().c_str(), dest_ip.unparse().c_str());
#endif

    // logging
    notify_route_cbs(packet, dest_ip, GRCB::FallbackToGF, 0, 0);

    if (_log)
      _log->log_no_route(packet, Timestamp::now());

    output(2).push(packet);
  }
}

CLICK_ENDDECLS

EXPORT_ELEMENT(LookupLocalGridRoute)

/*
 * lookupgeogridroute.{cc,hh} -- Grid geographic routing element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <stddef.h>
#include <click/config.h>
#include "lookupgeogridroute.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ether.h>
#include <click/click_ip.h>
#include "elements/standard/scheduleinfo.hh"
#include "grid.hh"
#include <click/router.hh>
#include <click/glue.hh>
#include "filterbyrange.hh"

LookupGeographicGridRoute::LookupGeographicGridRoute()
  : Element(1, 3), _rt(0), _task(this)
{
  MOD_INC_USE_COUNT;
}

LookupGeographicGridRoute::~LookupGeographicGridRoute()
{
  MOD_DEC_USE_COUNT;
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
LookupGeographicGridRoute::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
                        cpElement, "GridRouteTable element", &_rt,
			0);
  return res;
}

int
LookupGeographicGridRoute::initialize(ErrorHandler *errh)
{

  if(_rt && _rt->cast("GridRouteTable") == 0){
    errh->warning("%s: GridRouteTable argument %s has the wrong type",
                  id().cc(),
                  _rt->id().cc());
    _rt = 0;
  } else if (_rt == 0) {
    errh->warning("%s: no GridRouteTable element given",
                  id().cc());
  }

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
LookupGeographicGridRoute::run_scheduled()
{
  if (Packet *p = input(0).pull())
    push(0, p); 
  _task.fast_reschedule();
}

void
LookupGeographicGridRoute::push(int port, Packet *packet)
{
  /*
   * expects packets with MAC header and Grid NBR_ENCAP header
   */
  assert(packet);
  assert(port == 0);

  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));

  /*
   * send unknown packet type out error output
   */
  if (gh->type != grid_hdr::GRID_NBR_ENCAP &&
      gh->type != grid_hdr::GRID_LOC_REPLY) {
    click_chatter("LookupGeographicGridRoute %s: received unexpected Grid packet type: %s", 
		  id().cc(), grid_hdr::type_string(gh->type).cc());
    output(2).push(packet);
    return;
  }

  struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + gh->hdr_len);
  /*
   * drop packet meant for us; someone else should have already handled it
   */
  IPAddress dest_ip(encap->dst_ip);
  if (dest_ip == _ipaddr) {
    click_chatter("LookupGeographicGridroute %s: got an IP packet for us %s, dropping it",
		  id().cc(),
		  dest_ip.s().cc());
    packet->kill();
    return;
  }

  
  if (_rt == 0) {
    // no UpdateGridRoutes next-hop table in configuration
    click_chatter("LookupGeographicGridRoute %s: can't forward packet for %s; there is no routing table", id().cc(), dest_ip.s().cc());
    output(1).push(packet);
    return;
  }

  if (!encap->dst_loc_good) {
    click_chatter("LookupGeographicGridroute %s: bad destination location in packet for %s",
                  id().cc(),
                  dest_ip.s().cc());
    output(2).push(packet);
    return;
  }

  WritablePacket *xp = packet->uniqueify();
  /*
   * This code will update the hop count, tx ip (us) and dst/src MAC
   * addresses.  
   */
  EtherAddress next_hop_eth;
  bool found_next_hop = get_next_geographic_hop(dest_ip, encap->dst_loc, &next_hop_eth);

  if (found_next_hop) {
    struct click_ether *eh = (click_ether *) xp->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth.data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (xp->data() + sizeof(click_ether));
    gh->tx_ip = _ipaddr;
    encap->hops_travelled++;
    // leave src location update to FixSrcLoc element
    output(0).push(xp);
  }
  else {
#if 0
    click_chatter("LookupGeographicGridRoute %s: unable to forward packet for %s with geographic routing", id().cc(), dest_ip.s().cc());
    int ip_off = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap);
    xp->pull(ip_off);
    IPAddress src_ip(xp->data() + 12);
    IPAddress dst_ip(xp->data() + 16);
    unsigned short *sp = (unsigned short *) (xp->data() + 20);
    unsigned short *dp = (unsigned short *) (xp->data() + 22);
    unsigned short src_port = ntohs(*sp);
    unsigned short dst_port = ntohs(*dp);
    click_chatter("packet info: %s:%hu -> %s:%hu", src_ip.s().cc(), src_port, dst_ip.s().cc(), dst_port);
#endif
    output(1).push(xp);
  }
}


bool 
LookupGeographicGridRoute::get_next_geographic_hop(IPAddress, grid_location dest_loc, 
						   EtherAddress *dest_eth) const
{
  /*
   * search through table for all nodes we have routes to and for whom
   * we know the position.  of these, choose the node closest to the
   * destination location to send the packet to.  
   */
  IPAddress next_hop;
  double d = 0;
  bool found_one = false;
  for (GridRouteTable::RTIter iter = _rt->_rtes.first(); iter; iter++) {
    const GridRouteTable::RTEntry &rte = iter.value();
    if (!rte.loc_good)
      continue; // negative err means don't believe info at all
    double new_d = FilterByRange::calc_range(dest_loc, rte.loc);
    if (!found_one) {
      found_one = true;
      d = new_d;
      next_hop = rte.next_hop_ip;
    }
    else if (new_d < d) {
      d = new_d;
      next_hop = rte.next_hop_ip;
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

  // find the MAC address
  GridRouteTable::RTEntry *nent = _rt->_rtes.findp(next_hop);
  if (nent == 0) {
    click_chatter("%s: dude, routing table is not consistent -- there is no entry for the next hop", id().cc());
    return false;
  }
  if (nent->num_hops != 1) {
    click_chatter("%s: dude, routing table is not consistent -- the next hop entry is not one hop away", id().cc());
    return false;
  }
  *dest_eth = nent->next_hop_eth;
  return true;
} 



LookupGeographicGridRoute *
LookupGeographicGridRoute::clone() const
{
  return new LookupGeographicGridRoute;
}


void
LookupGeographicGridRoute::add_handlers()
{
  add_default_handlers(true);
}

/* XXX I feel like there is a general pattern here of filling in the
 * packet based on some information looked up in the routing table.
 * could get all generic here and provide an interface to the table
 * for generic visitors to pull out desired info, and a generic
 * ``lookup-and-modify-packet'' element that lets me plug in the
 * appropriate visitors... i guess i could use the iterators... */


ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LookupGeographicGridRoute)

/*
 * lookuplocalgridroute.{cc,hh} -- Grid multihop local routing element
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "lookuplocalgridroute.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ether.h>
#include <click/click_ip.h>
#include "elements/standard/scheduleinfo.hh"
#include "grid.hh"
#include <click/router.hh>
#include <click/glue.hh>

LookupLocalGridRoute::LookupLocalGridRoute() : Element(2, 4), _nbr(0)
{
  MOD_INC_USE_COUNT;
}

LookupLocalGridRoute::~LookupLocalGridRoute()
{
  MOD_DEC_USE_COUNT;
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
LookupLocalGridRoute::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
                        cpElement, "UpdadateGridRoutes element", &_nbr,
			0);
  return res;
}

int
LookupLocalGridRoute::initialize(ErrorHandler *errh)
{

  if(_nbr && _nbr->cast("UpdateGridRoutes") == 0){
    errh->warning("%s: UpdateGridRoutes argument %s has the wrong type",
                  id().cc(),
                  _nbr->id().cc());
    _nbr = 0;
  } else if (_nbr == 0) {
    errh->warning("%s: no UpdateGridRoutes element given",
                  id().cc());
  }

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
LookupLocalGridRoute::run_scheduled()
{
  if (Packet *p = input(0).pull())
    push(0, p); 
  reschedule();

}

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
      {/* 
	* try to either receive the packet or forward it
	*/
	struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + gh->hdr_len);
	IPAddress dest_ip(encap->dst_ip);
	if (dest_ip == _ipaddr) {
	  // it's for us, send to higher level
#if 0
	  click_chatter("%s: got an IP packet for us %s",
                        id().cc(),
                        dest_ip.s().cc());
#endif
	  packet->pull(sizeof(click_ether) + gh->hdr_len + sizeof(grid_nbr_encap)); 
	  output(1).push(packet);
	  break;
	}
	else {
	  // try to forward it!
	  forward_grid_packet(packet, encap->dst_ip);
	}
      }
      break;

    default:
      click_chatter("%s: received unexpected Grid packet type: %s", 
		    id().cc(), grid_hdr::type_string(gh->type).cc());
      output(3).push(packet);
    }
  }
  else {
    /*
     * input from higher level protocol -- expects IP packets
     * annotated with src IP address.
     */
    assert(port == 1);
    // check to see is the desired dest is our neighbor
    IPAddress dst = packet->dst_ip_anno();
    if (dst == _ipaddr) {
      click_chatter("%s: got IP packet from us for our address; looping it back.  Check the configuration.", id().cc());
      output(1).push(packet);
    }
    else {
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

      forward_grid_packet(new_packet, dst);
    }
  }
}

LookupLocalGridRoute *
LookupLocalGridRoute::clone() const
{
  return new LookupLocalGridRoute;
}


void
LookupLocalGridRoute::add_handlers()
{
  add_default_handlers(true);
}

bool
LookupLocalGridRoute::get_next_hop(IPAddress dest_ip, EtherAddress *dest_eth) const
{
  assert(dest_eth != 0);

  // is the destination an immediate nbr?
  UpdateGridRoutes::NbrEntry *ne = _nbr->_addresses.findp(dest_ip);
  if (ne != 0) {
#if 0
    click_chatter("%s: found immediate nbr %s for next hop for %s",
                  id().cc(),
                  ne->ip.s().cc(),
                  dest_ip.s().cc());
#endif
    *dest_eth = ne->eth;
    return true;
  }
  if (ne == 0) {
    // not an immediate nbr, search multihop nbrs
    UpdateGridRoutes::far_entry *fe = _nbr->_rtes.findp(dest_ip);
    if (fe != 0) {
      // we know how to get to this dest, look up MAC addr for next hop
      ne = _nbr->_addresses.findp(IPAddress(fe->nbr.next_hop_ip));
      if (ne != 0) {
	*dest_eth = ne->eth;
#if 0
	click_chatter("%s: trying to use next hop %s for %s",
		      id().cc(),
		      ne->ip.s().cc(),
		      dest_ip.s().cc());
#endif
	return true;
      }
      else {
	click_chatter("%s: dude, MAC nbr table and routing table are not consistent!", id().cc());
      }
    }
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
   * not be touched, so if we are orinigating the packet, those have
   * to be setup before calling this function.  
   */

  if (_nbr == 0) {
    // no UpdateGridRoutes next-hop table in configuration
    click_chatter("%s: can't forward packet for %s; there is no routing table, trying geographic forwarding", id().cc(), dest_ip.s().cc());
    output(2).push(packet);
    xp->kill();
    return;
  }

  struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));

  EtherAddress next_hop_eth;
  bool found_next_hop = get_next_hop(dest_ip, &next_hop_eth);

  if (found_next_hop) {
    struct click_ether *eh = (click_ether *) packet->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth.data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
    gh->tx_ip = _ipaddr;
    encap->hops_travelled++;
    // leave src location update to FixSrcLoc element
    output(0).push(packet);
  }
  else {
    // click_chatter("%s: unable to forward packet for %s with local routing, trying geographic routing", id().cc(), dest_ip.s().cc());
    output(2).push(packet);
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LookupLocalGridRoute)

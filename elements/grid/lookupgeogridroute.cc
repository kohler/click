/*
 * lookupgeogridroute.{cc,hh} -- Grid geographic routing element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <stddef.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "lookupgeogridroute.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ether.h"
#include "click_ip.h"
#include "elements/standard/scheduleinfo.hh"
#include "grid.hh"
#include "router.hh"
#include "glue.hh"

LookupGeographicGridRoute::LookupGeographicGridRoute() : Element(1, 3), _rt(0)
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
LookupGeographicGridRoute::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
                        cpElement, "UpdadateGridRoutes element", &_rt,
			0);
  return res;
}

int
LookupGeographicGridRoute::initialize(ErrorHandler *errh)
{

  if(_rt && _rt->cast("UpdateGridRoutes") == 0){
    errh->warning("%s: UpdateGridRoutes argument %s has the wrong type",
                  id().cc(),
                  _rt->id().cc());
    _rt = 0;
  } else if (_rt == 0) {
    errh->warning("%s: no UpdateGridRoutes element given",
                  id().cc());
  }

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
LookupGeographicGridRoute::run_scheduled()
{
  if (Packet *p = input(0).pull())
    push(0, p); 
  reschedule();

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
  if (gh->type != grid_hdr::GRID_NBR_ENCAP) {
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

  if (encap->dst_loc_err < 0) {
    click_chatter("LookupGeographicGridroute %s: bad destination location in packet for %s",
                  id().cc(),
                  dest_ip.s().cc());
    output(2).push(packet);
    return;
  }

  WritablePacket *xp = packet->uniqueify();
  /*
   * This code will update the hop count, src ip (us) and dst/src MAC
   * addresses.  
   */
  EtherAddress next_hop_eth;
  bool found_next_hop = _rt->get_next_geographic_hop(dest_ip, encap->dst_loc, &next_hop_eth);

  if (found_next_hop) {
    struct click_ether *eh = (click_ether *) xp->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth.data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (xp->data() + sizeof(click_ether));
    gh->ip = _ipaddr.addr();
    encap->hops_travelled++;
    // leave src location update to FixSrcLoc element
    output(0).push(xp);
  }
  else {
    click_chatter("LookupGeographicGridRoute %s: unable to forward packet for %s with geographics routing", id().cc(), dest_ip.s().cc());
    output(2).push(xp);
  }
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

void
LookupGeographicGridRoute::forward_grid_packet(Packet *, IPAddress)
{
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LookupGeographicGridRoute)

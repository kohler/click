/*
 * gridprobehandler.{cc,hh} -- element that handles Grid probe packets
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "gridprobehandler.hh"
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include "grid.hh"


GridProbeHandler::GridProbeHandler() 
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  add_output();
}

int
GridProbeHandler::initialize(ErrorHandler *)
{
  return 0;
}

GridProbeHandler::~GridProbeHandler()
{
  MOD_DEC_USE_COUNT;
}


GridProbeHandler *
GridProbeHandler::clone() const
{
  return new GridProbeHandler;
}

int
GridProbeHandler::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpEthernetAddress, "Ethernet address", &_eth,
		     cpIPAddress, "IP address", &_ip,
		     0);
}


void
GridProbeHandler::push(int port, Packet *p)
{
  assert(port == 0);
  assert(p);

  click_ether *e = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (e + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);
  grid_route_probe *rp = (grid_route_probe *) (nb + 1);

  if (gh->type != grid_hdr::GRID_ROUTE_PROBE) {
    click_chatter("GridProbeHandler %s: received unexpected Grid packet type %s; is the configuration wrong?",
		  id().cc(), grid_hdr::type_string(gh->type).cc());
    p->kill();
    return;
  }

  /* build new probe packet */
  grid_hdr *gh2;
  grid_nbr_encap *nb2;
  grid_route_reply *rr;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*gh2) + sizeof(*nb2) + sizeof(*rr) + 2);
  q->pull(2);
  
  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
    q->set_timestamp_anno(tv);

  memset(q->data(), 0, q->length());
  e = (click_ether *) q->data();
  gh2 = (grid_hdr *) (e + 1);
  nb2 = (grid_nbr_encap *) (gh2 + 1);
  rr = (grid_route_reply *) (nb2 + 1);

  e->ether_type = htons(ETHERTYPE_GRID);
  // leave ether src, dest for the forwarding elements to fill in

  gh2->hdr_len = sizeof(grid_hdr);
  gh2->type = grid_hdr::GRID_ROUTE_REPLY;
  gh2->ip = _ip;
  gh2->total_len = htons(q->length() - sizeof(click_ether));

  nb2->dst_ip = gh->ip;
  nb2->dst_loc_good = false;
  nb2->hops_travelled = 0;

  rr->nonce = rp->nonce; /* keep in net byte order */
  rr->probe_dest = nb->dst_ip;
  rr->reply_hop = nb->hops_travelled;

  output(1).push(q);

  /* pass through probe if we aren't the destination */
  if (_ip != nb->dst_ip)
    output(0).push(p);
  else
    p->kill();
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridProbeHandler)

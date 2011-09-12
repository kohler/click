/*
 * gridprobesender.{cc,hh} -- element that produces Grid route probe packets
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
#include "gridprobesender.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "grid.hh"
CLICK_DECLS

GridProbeSender::GridProbeSender()
{
}

int
GridProbeSender::initialize(ErrorHandler *)
{
  return 0;
}

GridProbeSender::~GridProbeSender()
{
}


int
GridProbeSender::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ETH", _eth)
	.read_mp("IP", _ip)
	.complete();
}

void
GridProbeSender::send_probe(IPAddress &ip, unsigned int nonce)
{
  click_ether *e;
  grid_hdr *gh;
  grid_nbr_encap *nb;
  grid_route_probe *rp;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*gh) + sizeof(*nb) + sizeof(*rp) + 2);
  q->pull(2);

  q->set_timestamp_anno(Timestamp::now());

  memset(q->data(), 0, q->length());
  e = (click_ether *) q->data();
  gh = (grid_hdr *) (e + 1);
  nb = (grid_nbr_encap *) (gh + 1);
  rp = (grid_route_probe *) (nb + 1);

  e->ether_type = htons(ETHERTYPE_GRID);
  // leave ether src, dest for the forwarding elements to fill in

  gh->hdr_len = sizeof(grid_hdr);
  gh->type = grid_hdr::GRID_ROUTE_PROBE;
  gh->ip = _ip;
  gh->total_len = htons(q->length() - sizeof(click_ether));

  nb->dst_ip = ip;
#ifndef SMALL_GRID_HEADERS
  nb->dst_loc_good = false;
#endif
  nb->hops_travelled = 0;

  rp->nonce = htonl(nonce);
  rp->send_time.tv_sec = htonl(q->timestamp_anno().sec());
  rp->send_time.tv_usec = htonl(q->timestamp_anno().usec());

  output(0).push(q);
}


static int
probe_write_handler(const String &arg, Element *element,
		    void *, ErrorHandler *errh)
{
  GridProbeSender *l = (GridProbeSender *) element;

  Vector<String> arg_list;
  cp_argvec(arg, arg_list);

  IPAddress ip;
  unsigned int nonce;
  int res = Args(arg_list, element, errh)
      .read_mp("IP", ip)
      .read_mp("NONCE", nonce)
      .complete();
  if (res < 0)
    return res;

  l->send_probe(ip, nonce);
  return 0;
}


void
GridProbeSender::add_handlers()
{
  add_write_handler("send_probe", probe_write_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridProbeSender)

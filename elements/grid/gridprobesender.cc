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
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "grid.hh"


GridProbeSender::GridProbeSender() 
{
  MOD_INC_USE_COUNT;
  add_output();
}

int
GridProbeSender::initialize(ErrorHandler *)
{
  return 0;
}

GridProbeSender::~GridProbeSender()
{
  MOD_DEC_USE_COUNT;
}


GridProbeSender *
GridProbeSender::clone() const
{
  return new GridProbeSender;
}

int
GridProbeSender::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpEthernetAddress, "Ethernet address", &_eth,
		     cpIPAddress, "IP address", &_ip,
		     0);
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
  
  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
    q->set_timestamp_anno(tv);

  memset(q->data(), 0, q->length());
  e = (click_ether *) q->data();
  gh = (grid_hdr *) (e + 1);
  nb = (grid_nbr_encap *) (gh + 1);
  rp = (grid_route_probe *) (nb + 1);

  e->ether_type = htons(ETHERTYPE_GRID);
  // leave ether src, dest for the forwarding elements to fill in

  gh->hdr_len = sizeof(grid_hdr);
  gh->type = grid_hdr::GRID_LOC_REPLY;
  gh->ip = _ip;
  gh->total_len = htons(q->length() - sizeof(click_ether));

  nb->dst_ip = ip;
  nb->dst_loc_good = false;
  nb->hops_travelled = 0;

  rp->nonce = htonl(nonce);

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
  cp_va_parse(arg_list, element, errh,
	      cpIPAddress, "IP address", &ip,
	      cpUnsigned, "Nonce (unsigned int)", &nonce,
	      0);

  l->send_probe(ip, nonce);
  return 0;
}


void
GridProbeSender::add_handlers()
{
  add_default_handlers(true);
  add_write_handler("send_probe", probe_write_handler, (void *) 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridProbeSender)

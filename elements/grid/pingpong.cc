/*
 * pingpong.{cc,hh} -- ping-pong link quality/strength stats back to link sender
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
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
#include <clicknet/ether.h>
#include "pingpong.hh"
#include <click/glue.hh>
#include "grid.hh"
#include "linkstat.hh"
CLICK_DECLS

PingPong::PingPong()
{
}

PingPong::~PingPong()
{
}


int
PingPong::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("LINKSTAT", reinterpret_cast<Element *&>(_ls))
	.complete();
}



int
PingPong::initialize(ErrorHandler *)
{
  return 0;
}


Packet *
PingPong::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE:
  case grid_hdr::GRID_ROUTE_REPLY: {
#ifndef SMALL_GRID_HEADERS
    grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);

    nb->link_qual = 0;
    nb->link_sig = 0;
    nb->measurement_time.tv_sec = nb->measurement_time.tv_usec = 0;

    EtherAddress eth(eh->ether_dhost);
    LinkStat::stat_t *s = _ls->_stats.findp(eth);
    if (s) {
      nb->link_qual = htonl(s->qual);
      nb->link_sig = htonl(s->sig);
      nb->measurement_time.tv_sec = htonl(s->when.tv_sec);
      nb->measurement_time.tv_usec = htonl(s->when.tv_usec);
    }
    else
      click_chatter("PingPong: error!  unable to get signal strength or quality info for one-hop neighbor %s\n",
		    IPAddress(nb->dst_ip).unparse().c_str());

    nb->num_rx = 0;
    nb->num_expected = 0;
    nb->last_bcast.tv_sec = nb->last_bcast.tv_usec = 0;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = _ls->get_bcast_stats(eth, nb->last_bcast, window, num_rx, num_expected);
    if (res) {
      if (num_rx > 255 || num_expected > 255) {
	click_chatter("PingPong: error! overflow on broadcast loss stats for one-hop neighbor %s",
		      IPAddress(nb->dst_ip).unparse().c_str());
	num_rx = num_expected = 255;
      }
      nb->num_rx = num_rx;
      nb->num_expected = num_expected;
      nb->last_bcast = hton(nb->last_bcast);
    }
#endif
  }
  break;
  default:
    ;
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PingPong)

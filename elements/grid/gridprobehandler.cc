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


GridProbeHandler::GridProbeHandler() : 
  _gf_cb_id(-1), _fq_cb_id(-1), _lr_cb_id(-1), 
  _lr_el(0), _gf_el(0), _fq_el(0), 
  _cached_reply_pkt(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  add_output();
}

int
GridProbeHandler::initialize(ErrorHandler *errh)
{
  if (!_lr_el || !_lr_el->cast("LookupLocalGridRoute")) {
    errh->warning("%s: LookupLocalGridRoute argument is missing or has the wrong type, probe replies will not contain all info",
		  id().cc());
    _lr_el = 0;
  }
  if (!_gf_el || !_gf_el->cast("LookupGeographicGridRoute")) {
    errh->warning("%s: LookupGeographicGridRoute argument is missing or has the wrong type, probe replies will not contain all info",
		  id().cc());
    _gf_el = 0;
  }
  if (!_fq_el || !_gf_el->cast("FloodingLocQuerier")) {
    errh->warning("%s: FloodingLocQuerier argument is missing or has the wrong type, probe replies will not contain all info",
		  id().cc());
    _fq_el = 0;
  }

  if (_lr_el) 
    _lr_cb_id = _lr_el->add_callback(this);
  if (_gf_el)
    _gf_cb_id = _gf_el->add_callback(this);
  if (_fq_el)
    _gf_cb_id = _fq_el->add_callback(this);

  if (_lr_cb_id < 0) 
    errh->warning("%s: unable to install local routing action callback, probe replies will not contain all info",
		  id().cc());
  if (_gf_cb_id < 0) 
    errh->warning("%s: unable to install geographic forwarding action callback, probe replies will not contain all info",
		  id().cc());
  if (_fq_cb_id < 0) 
    errh->warning("%s: unable to loc query action callback, probe replies will not contain all info",
		  id().cc());
  
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
		     cpOptional,
		     cpElement, "LookupLocalGridRoute element", &_lr_el,
		     cpElement, "LookupGeographicsGridRoute element", &_gf_el,
		     cpElement, "FloodingLocQuerier element", &_fq_el,
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
  rr->probe_send_time = rp->send_time;

  rr->probe_dest = nb->dst_ip;
  rr->reply_hop = nb->hops_travelled;


  if (_cached_reply_pkt != 0) {
    click_chatter("GridProbeHandler: error!!! cached reply packet was deleted before being sent; the appropriate route action callback was not received\n");
    _cached_reply_pkt->kill();
    _cached_reply_pkt = 0;
  }
  
  /* before figuring out what to do with the cached reply, check to
     see if the callback stuff is set up okay */
  if (_lr_el && _gf_el && _fq_el 
      && _lr_cb_id > -1 && _gf_cb_id > -1 && _fq_cb_id > -1) {
    /* yes, the callbacks are cool */
    if (_ip != nb->dst_ip) {
      /* probe should be forwarded, cache reply and wait for callbacks
         after forwarding decision */
      _cached_reply_pkt = q;
      set_route_cb_bit(p, _gf_cb_id);
      set_route_cb_bit(p, _lr_cb_id);
    }
    else {
      /* we are probe's final dest, there will be no routing action
         callback -- so send reply now */
      rr->route_action = htonl(ProbeFinished);
      _cached_reply_pkt = 0;
      output(1).push(q);
    }  
  }
  else {
    /* the route action callbacks are f-ed up, so just send the reply
       packet anyway */
    rr->route_action = htonl(UnknownAction);
    _cached_reply_pkt = 0;
    output(1).push(q);
  }
  

  /* pass through probe if we aren't the destination */
  if (_ip != nb->dst_ip) 
    output(0).push(p);
  else 
    p->kill();
}

void
GridProbeHandler::route_cb(int id, unsigned int dest_ip, Action a, unsigned int data, unsigned int data2)
{
  if (id != _lr_cb_id && id != _gf_cb_id && id != _fq_cb_id) {
    click_chatter("GridProbeHandler: error!!! route action callback invoked with the wrong callback id\n");
    if (_cached_reply_pkt) {
      _cached_reply_pkt->kill();
      _cached_reply_pkt = 0;
      return;
    }
  }

  if (!_cached_reply_pkt) {
    click_chatter("GridProbeHandler: error!!! route action callback invoked, but there is no probe reply cached\n");
    return;
  }  

  

  grid_route_reply *rr = (grid_route_reply *) (_cached_reply_pkt->data() + sizeof(click_ether) 
					      + sizeof(grid_hdr) + sizeof(grid_nbr_encap));
  if (rr->probe_dest != dest_ip) {
    click_chatter("GridProbeHandler: error!!! route action callback probe dest arg does not match cached reply\n");
    _cached_reply_pkt->kill();
    _cached_reply_pkt = 0;
    return;
  }

  rr->route_action = htonl(a);
  rr->data1 = htonl(data);
  rr->data2 = htonl(data2);

  switch (a) {
  case SendToIP:
  case ForwardDSDV:
  case ForwardGF:
  case QueuedForLocQuery:
  case Drop:
    output(1).push(_cached_reply_pkt);
    _cached_reply_pkt = 0;
    break;
  case FallbackToGF:
  case NoLocQueryNeeded:
  case CachedLocFound:
    /* XXX could perhaps do some more sanity checking here */
    /* these cases don't merit action, as we expect to receive another callback for a later action */
    break;
  case ProbeFinished:
    click_chatter("GridProbeHandler: error!!! route action callback invoked with ProbeFinished action, but GridProbeHandler should kill probe and send reply in this case\n", a);
  case UnknownAction:
  default:
    click_chatter("GridProbeHandler: error!!! route action callback invoked with an unknown action (%d), sending reply now\n", a);
    output(1).push(_cached_reply_pkt);
    _cached_reply_pkt = 0;
    break;
  }
}


ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridProbeHandler)

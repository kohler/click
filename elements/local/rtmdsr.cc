/*
 * RTMDSR.{cc,hh} -- toy DSR implementation
 * Robert Morris
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
#include "rtmdsr.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

RTMDSR::RTMDSR()
  : _timer(this)
{
  MOD_INC_USE_COUNT;

  add_input();
  add_input();
  add_output();
}

RTMDSR::~RTMDSR()
{
  MOD_DEC_USE_COUNT;
}

int
RTMDSR::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		     cpIPAddress, "IP address", &_ip,
		     0);
  return ret;
}

RTMDSR *
RTMDSR::clone () const
{
  return new RTMDSR;
}

int
RTMDSR::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
RTMDSR::run_timer ()
{
  _timer.schedule_after_ms(1000);
}

RTMDSR::Dst *
RTMDSR::find_dst(IPAddress ip, bool create)
{
  int i;

  for(i = 0; i < _dsts.size(); i++)
    if(_dsts[i]._ip == ip)
      return & _dsts[i];

  if(create){
    _dsts.push_back(Dst(ip));
    Dst *d = find_dst(ip, false);
    assert(d);
    return d;
  }

  return 0;
}

RTMDSR::Route *
RTMDSR::best_route(Dst *d)
{
  int i;
  int bm = -1;
  int bi = -1;

  for(i = 0; i < d->_routes.size(); i++){
    if(bi == -1 || d->_routes[i]._pathmetric < bm){
      bi = i;
      bm = d->_routes[i]._pathmetric;
    }
  }

  if(bi != -1)
    return & d->_routes[bi];
  return 0;
}

void
RTMDSR::start_query(Dst *d)
{
  time_t now;
  time(&now);
  if(d->_when != 0 && now < d->_when + 10){
    // We sent a query less than 10 seconds ago, don't repeat.
    return;
  }

  char buf[1024];
  memset(buf, '\0', sizeof(buf));
  struct pkt *pk = (struct pkt *) buf;
  pk->_type = (PacketType) htonl(PT_QUERY);
  pk->_dst = d->_ip;
  pk->_metric = 0;
  pk->_seq = htonl(d->_seq + 1);
  pk->_nhops = htonl(1);
  pk->_hops[0] = _ip.in_addr();
  int len = pk->len();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  memcpy(p->data(), (const void *) pk, len);
  output(0).push(p);

  d->_seq += 1;
  d->_when = now;
}

// Process a packet from the net, sent by a different RTMDSR.
void
RTMDSR::got_pkt(Packet *p_in)
{
  if(p_in->length() < sizeof(int))
    return;

  struct pkt *pk = (struct pkt *) p_in->data();
  if(pk->_type == (PacketType) htonl(PT_QUERY)){
    click_chatter("DSR %s: query dst=%s nh=%d",
                  _ip.s().cc(),
                  IPAddress(pk->_dst).s().cc(),
                  ntohl(pk->_nhops));
  } else {
    click_chatter("DSR %s: odd type %x",
                  _ip.s().cc(),
                  ntohl(pk->_type));
  }
}

void
RTMDSR::push(int port, Packet *p_in)
{
  if(port == 0){
    // Packet from upper layers in same host.
    Dst *d = find_dst(p_in->dst_ip_anno(), true);
    Route *r = best_route(d);
    click_chatter("DSR %s: dst %s %x %x",
                  _ip.s().cc(),
                  p_in->dst_ip_anno().s().cc(),
                  d,
                  r);
    if(r == 0)
      start_query(d);
  } else {
    got_pkt(p_in);
  }
  p_in->kill();
}

// generate Vector template instance
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<RTMDSR::Dst>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(RTMDSR)

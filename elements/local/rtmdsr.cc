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
  add_output();
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
  // Delete stale entries from list of queries we've seen.
  time_t now = time();
  Vector<Seen> v;
  int i;
  for(i = 0; i < _seen.size(); i++)
    if(_seen[i]._when + QueryLife >= now)
      v.push_back(_seen[i]);
  _seen = v;

  _timer.schedule_after_ms(1000);
}

// Returns an index into the _dsts[] array, or -1.
int
RTMDSR::find_dst(IPAddress ip, bool create)
{
  int i;

  for(i = 0; i < _dsts.size(); i++)
    if(_dsts[i]._ip == ip)
      return i;

  if(create){
    _dsts.push_back(Dst(ip));
    i = find_dst(ip, false);
    assert(i >= 0);
    return i;
  }

  return -1;
}

// Returns the best route, or a dummy zero-hop route.
RTMDSR::Route &
RTMDSR::best_route(IPAddress dstip)
{
  static Route junk;
  int i;
  int bm = -1;
  int bi = -1;

  int di = find_dst(dstip, false);
  if(di < 0)
    return junk; // Oops

  Dst &d = _dsts[di];
  for(i = 0; i < d._routes.size(); i++){
    if(bi == -1 || d._routes[i]._pathmetric < bm){
      bi = i;
      bm = d._routes[i]._pathmetric;
    }
  }

  if(bi != -1)
    return d._routes[bi];
  return junk; // Oops
}

time_t
RTMDSR::time()
{
  timeval tv;
  click_gettimeofday(&tv);
  return tv.tv_sec;
}

void
RTMDSR::start_data(const u_char *payload, u_long payload_len, Route &r)
{
  int hops = r._hops.size() - 1; // Not this node.
  int len = pkt::len1(hops, payload_len);
  WritablePacket *p = Packet::make(len);
  struct pkt *pk = (struct pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = htonl(PT_DATA);
  pk->_dlen = htons(payload_len);
  pk->_nhops = htons(hops);
  pk->_next = htons(0);
  int i;
  for(i = 1; i < hops + 1; i++)
    pk->_hops[i-1] = r._hops[i]._ip.in_addr();
  memcpy(pk->data(), payload, payload_len);

  send(p);
}

// Got a data packet whose ultimate destination is us.
// Emit to upper layer.
void
RTMDSR::got_data(struct pkt *pk)
{
  click_chatter("DSR %s: rcvd data from net for us",
                _ip.s().cc());
  Packet *p = Packet::make(pk->data(), pk->_dlen);
  output(0).push(p);
}

void
RTMDSR::start_query(IPAddress dstip)
{
  int di = find_dst(dstip, true);
  Dst &d = _dsts[di];  

  time_t now = time();
  if(d._when != 0 && now < d._when + QueryInterval){
    // We sent a query less than 10 seconds ago, don't repeat.
    return;
  }

  int len = pkt::hlen1(1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct pkt *pk = (struct pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = htonl(PT_QUERY);
  pk->_qdst = d._ip;
  pk->_seq = htonl(d._seq + 1);
  pk->_nhops = htons(1);
  pk->_hops[0] = _ip.in_addr();
  
  d._seq += 1;
  d._when = now;
  _seen.push_back(Seen(_ip.in_addr(), pk->_seq));

  send(p);
}

// Forward a query, data, or reply packet.
// Makes a copy.
// _next should point to this node; forward() adjusts
// it to point to the next node.
void
RTMDSR::forward(const struct pkt *pk1)
{
  u_long type = ntohl(pk1->_type);
  u_short next = ntohs(pk1->_next);
  u_short nhops = ntohs(pk1->_nhops);
  if(type == PT_REPLY){
    next = next - 1;
  } else if(type == PT_DATA){
    next = next + 1;
  } else {
    assert(0);
  }

  if(next >= nhops)
    return;

  int len = pk1->len();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct pkt *pk = (struct pkt *) p->data();
  memcpy(pk, pk1, len);

  pk->_next = htons(next);

  send(p);
}

// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
RTMDSR::send(Packet *p)
{
  const struct pkt *pk = (const struct pkt *) p->data();
  struct in_addr nxt;
  int outnum;

  u_long type = ntohl(pk->_type);
  if(type == PT_QUERY){
    nxt.s_addr = 0xffffffff;
    outnum = 1;
  } else if(type == PT_REPLY || type == PT_DATA){
    u_short next = ntohs(pk->_next);
    nxt = pk->_hops[next];
    outnum = 2;
  } else {
    assert(0);
    return;
  }

  p->set_dst_ip_anno(nxt); // For ARP.
  output(outnum).push(p);
}

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
RTMDSR::forward_query(struct pkt *pk1)
{
  IPAddress src(pk1->_hops[0]);
  int i;
  for(i = 0; i < _seen.size(); i++){
    if(src == _seen[i]._src &&
       pk1->_seq == _seen[i]._seq){
      _seen[i]._when = time();
      return;
    }
  }

  if(_seen.size() > MaxSeen)
     return;
  _seen.push_back(Seen(src, pk1->_seq));

  u_short nhops = ntohs(pk1->_nhops);
  if(nhops > MaxHops)
    return;

  int len = pkt::hlen1(nhops + 1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct pkt *pk = (struct pkt *) p->data();
  memcpy(pk, pk1, len);

  pk->_nhops = htons(nhops + 1);
  pk->_hops[nhops] = _ip.in_addr();

  send(p);
}

// Continue unicasting a reply packet.
void
RTMDSR::forward_reply(struct pkt *pk)
{
  forward(pk);
}

// Continue unicasting a data packet.
void
RTMDSR::forward_data(struct pkt *pk)
{
  forward(pk);
}

String
RTMDSR::Route::s()
{
  char buf[50];
  sprintf(buf, "m=%d ", _pathmetric);
  String s(buf);
  int i;
  for(i = 0; i < _hops.size(); i++){
    s = s + _hops[i]._ip.s();
    if(i + 1 < _hops.size())
      s = s + " ";
  }
  return s;
}

void
RTMDSR::start_reply(struct pkt *pk1)
{
  int len = pk1->len();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct pkt *pk = (struct pkt *) p->data();
  
  memcpy(pk, pk1, len);
  pk->_type = htonl(PT_REPLY);
  int nh = ntohs(pk->_nhops);
  pk->_next = htons(nh - 1); // Indicates next hop.

  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
RTMDSR::got_reply(struct pkt *pk)
{
  int di = find_dst(pk->_qdst, false);
  if(di < 0){
    click_chatter("DSR %s: reply but no Dst %s",
                  _ip.s().cc(),
                  IPAddress(pk->_qdst).s().cc());
    return;
  }
  Dst &dst = _dsts[di];
  if(ntohl(pk->_seq) != dst._seq){
    click_chatter("DSR %s: reply but wrong seq %d %d",
                  _ip.s().cc(),
                  ntohl(pk->_seq),
                  dst._seq);
    return;
  }

  Route r;
  r._when = time();
  r._pathmetric = ntohs(pk->_nhops); // XXX
  int i;
  for(i = 0; i < ntohs(pk->_nhops); i++){
    r._hops.push_back(Hop(pk->_hops[i]));
  }
  r._hops.push_back(Hop(dst._ip));
  dst._routes.push_back(r);
  click_chatter("DSR %s: installed route to %s via [%s]",
                _ip.s().cc(),
                dst._ip.s().cc(),
                r.s().cc());
}

// Process a packet from the net, sent by a different RTMDSR.
void
RTMDSR::got_pkt(Packet *p_in)
{
  struct pkt *pk = (struct pkt *) p_in->data();
  if(p_in->length() < 20 || p_in->length() < pk->len()){
    click_chatter("DSR %s: bad pkt len %d",
                  _ip.s().cc(),
                  p_in->length());
    return;
  }

  u_long type = ntohl(pk->_type);
  u_short nhops = ntohs(pk->_nhops);
  u_short next = ntohs(pk->_next);

  if(type == PT_QUERY && nhops >= 1){
    click_chatter("DSR %s: query for %s from %s seq=%d hops=%d",
                  _ip.s().cc(),
                  IPAddress(pk->_qdst).s().cc(),
                  IPAddress(pk->_hops[0]).s().cc(),
                  ntohl(pk->_seq),
                  nhops);
    if(pk->_qdst == _ip.in_addr()){
      start_reply(pk);
    } else {
      forward_query(pk);
    }
  } else if(type == PT_REPLY && next < nhops){
    if(pk->_hops[next] != _ip.in_addr()){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("DSR %s: reply not for me %s",
                    _ip.s().cc(),
                    IPAddress(pk->_hops[next]).s().cc());
      return;
    }
    if(next == 0){
      // I'm the ultimate consumer of this reply. Add to routing tbl.
      got_reply(pk);
    } else {
      // Forward the reply.
      forward_reply(pk);
    }
  } else if(type == PT_DATA && next < nhops){
    if(pk->_hops[next] != _ip.in_addr()){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("DSR %s: data not for me %d/%d %s",
                    _ip.s().cc(),
                    ntohs(pk->_next),
                    ntohs(pk->_nhops),
                    IPAddress(pk->_hops[next]).s().cc());
      return;
    }
    if(next == nhops - 1){
      // I'm the ultimate consumer of this data.
      got_data(pk);
    } else {
      // Forward the data.
      forward_data(pk);
    }
  } else {
    click_chatter("DSR %s: bad pkt type=%x",
                  _ip.s().cc(),
                  type);
  }

  return;
}

void
RTMDSR::push(int port, Packet *p_in)
{
  if(port == 0){
    // Packet from upper layers in same host.
    Route &r = best_route(p_in->dst_ip_anno());
    click_chatter("DSR %s: data to %s via [%s]",
                  _ip.s().cc(),
                  p_in->dst_ip_anno().s().cc(),
                  r.s().cc());
    if(r._hops.size() > 0)
      start_data(p_in->data(), p_in->length(), r);
    else
      start_query(p_in->dst_ip_anno());
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

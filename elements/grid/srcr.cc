/*
 * SRCR.{cc,hh} -- DSR implementation
 * Robert Morris
 * John Bicket
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
#include "srcr.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef srcr_assert
#define srcr_assert(e) ((e) ? (void) 0 : srcr_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRCR::SRCR()
  :  _queries(0), _querybytes(0),
     _replies(0), _replybytes(0), _datas(0), _databytes(0),
     _timer(this), _link_stat(0)
{
  MOD_INC_USE_COUNT;

  add_input();
  add_input();
  add_output();
  add_output();

  MaxSeen = 200;
  MaxHops = 30;
  /**
   *  These are in milliseconds.
   */
  QueryInterval = 10000;
  QueryLife = 3000; 
  ARPLife = 30000;

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;
  
}

SRCR::~SRCR()
{
  MOD_DEC_USE_COUNT;
}

int
SRCR::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
                    cpIPAddress, "IP address", &_ip,
                    cpEthernetAddress, "Ethernet address", &_en,
		    cpElement, "LinkTable element", &_link_table,
                    cpKeywords,
                    "LS", cpElement, "LinkStat element", &_link_stat,
                    0);
  return ret;
}

SRCR *
SRCR::clone () const
{
  return new SRCR;
}

int
SRCR::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
SRCR::run_timer ()
{
  // Delete stale entries from list of queries we've seen.
  timeval now = get_timeval();
  Vector<Seen> v;
  int i;
  for(i = 0; i < _seen.size(); i++)
    if(timeval_past(add_millisec(_seen[i]._when, QueryLife),  now))
      v.push_back(_seen[i]);
  _seen = v;

  // Delete stale entries from our ARP-like cache.
  Vector<ARP> av;
  for(i = 0; i < _arp.size(); i++)
    if(timeval_past(add_millisec(_arp[i]._when, ARPLife), now))
      av.push_back(_arp[i]);
  _arp = av;

  _timer.schedule_after_ms(1000);
}

// Returns an index into the _dsts[] array, or -1.
int
SRCR::find_dst(IPAddress ip, bool create)
{
  int i;

  for(i = 0; i < _dsts.size(); i++)
    if(_dsts[i]._ip == ip)
      return i;

  if(create){
    _dsts.push_back(Dst(ip));
    i = find_dst(ip, false);
    srcr_assert(i >= 0);
    return i;
  }

  return -1;
}

timeval 
SRCR::get_timeval()
{
  timeval tv;
  click_gettimeofday(&tv);
  return tv;

}
bool
SRCR::timeval_past(timeval a, timeval b) 
{
  if (a.tv_sec == b.tv_sec) 
    return a.tv_usec > b.tv_usec;

  return a.tv_sec > b.tv_sec;

}

timeval
SRCR::add_millisec(timeval t, int milli)
 {
  timeval new_time;

  new_time = t;
  new_time.tv_usec += milli*1000;
  while (new_time.tv_usec > 1000000) {
    new_time.tv_sec++;
    new_time.tv_usec -= 1000000;
  }

  return new_time;
}
void
SRCR::start_data(const u_char *payload, u_long payload_len, Vector<IPAddress> r)
{
  int hops = r.size();
  int len = sr_pkt::len_with_data(hops, payload_len);
  WritablePacket *p = Packet::make(len);
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_DATA;
  pk->_dlen = htons(payload_len);
  pk->_nhops = htons(hops);
  pk->_next = htons(0);
  int i;
  for(i = 0; i < hops; i++) {
    pk->set_hop(i, r[i].in_addr());
  }
  memcpy(pk->data(), payload, payload_len);

  send(p);
}

// Got a data packet whose ultimate destination is us.
// Emit to upper layer.
void
SRCR::got_data(struct sr_pkt *pk)
{
  Packet *p = Packet::make(pk->data(), pk->_dlen);
  output(0).push(p);
}

void
SRCR::start_query(IPAddress dstip)
{
  int di = find_dst(dstip, true);
  Dst &d = _dsts[di];  

  timeval now = get_timeval();
  if(d._when.tv_sec != 0 && timeval_past(add_millisec(d._when, QueryInterval), now)){
    // We sent a query less than 10 seconds ago, don't repeat.
    return;
  }

  d._seq = ++_seq;

  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = d._ip;
  pk->_seq = htonl(d._seq);
  pk->_nhops = htons(1);
  pk->set_hop(0,_ip.in_addr());
  
  d._when = now;

  Vector<IPAddress> hops;
  Vector<u_short> metrics;
  hops.push_back(IPAddress(_ip));
  _seen.push_back(Seen(_ip.in_addr(), dstip, pk->_seq, now, 0, metrics, hops, 1));

  send(p);
}


// A packet has arrived from ip/en.
// Enter the pair in our ARP table.
void
SRCR::got_arp(IPAddress ip, u_char xen[6])
{
  EtherAddress en(xen);
  int i;
  for(i = 0; i < _arp.size(); i++){
    if(_arp[i]._ip == ip){
      if(_arp[i]._en != en){
        click_chatter("SRCR %s: got_arp %s changed from %s to %s",
                      _ip.s().cc(),
                      ip.s().cc(),
                      _arp[i]._en.s().cc(),
                      en.s().cc());
      }
      _arp[i]._en = en;
      _arp[i]._when = get_timeval();
      return;
    }
  }
      
  _arp.push_back(ARP(ip, en, get_timeval()));
}

// Given an IP address, look in our local ARP table to see
// if we happen to know that node's ethernet address.
// If we don't, just use ff:ff:ff:ff:ff:ff
bool
SRCR::find_arp(IPAddress ip, u_char en[6])
{
  int i;
  for(i = 0; i < _arp.size(); i++){
    if(_arp[i]._ip == ip){
      memcpy(en, _arp[i]._en.data(), 6);
      return true;
    }
  }
  click_chatter("SRCR %s: find_arp %s ???",
                _ip.s().cc(),
                ip.s().cc());
  memcpy(en, "\xff\xff\xff\xff\xff\xff", 6);
  return false;
}

// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
SRCR::send(WritablePacket *p)
{
  struct sr_pkt *pk = (struct sr_pkt *) p->data();

  pk->ether_type = ETHERTYPE_SRCR;
  memcpy(pk->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    memcpy(pk->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
    _queries++;
    _querybytes += p->length();
  } else if(type == PT_REPLY || type == PT_DATA){
    u_short next = ntohs(pk->_next);
    srcr_assert(next < MaxHops + 2);
    struct in_addr nxt = pk->get_hop(next);
    find_arp(IPAddress(nxt), pk->ether_dhost);
    if(type == PT_REPLY){
      _replies++;
      _replybytes += p->length();
    } else {
      _datas++;
      _databytes += p->length();
    }
  } else {
    srcr_assert(0);
    return;
  }

  output(1).push(p);
}

// Ask LinkStat for the metric for the link from other to us.
u_short
SRCR::get_metric(IPAddress other)
{
  u_short dft = 150; // default metric
  if(_link_stat){
    unsigned int tau;
    struct timeval tv;
    unsigned int frate, rrate;
    bool res = _link_stat->get_forward_rate(other, &frate, &tau, &tv);
    if(res == false)
      return dft;
    res = _link_stat->get_reverse_rate(other, &rrate, &tau);
    if(res == false)
      return dft;
    if(frate == 0 || rrate == 0)
      return dft;
    u_short m = 100 * 100 * 100 / (frate * (int) rrate);
    return m;
  } else {
    return dft;
  }
}

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
SRCR::process_query(struct sr_pkt *pk1)
{
  IPAddress src(pk1->get_hop(0));
  IPAddress dst(pk1->_qdst);
  u_short nhops = ntohs(pk1->_nhops) + 1;
  u_short last_hop_metric = get_metric(pk1->get_hop(nhops-1));
  u_short metric = 0;
  u_long seq = ntohl(pk1->_seq);
  int si;
  bool better = false;
  bool seen_before = false;
  bool already_forwarded = false;

  Vector<IPAddress> hops;
  for(int i = 0; i < nhops-1; i++) {
    IPAddress hop = IPAddress(pk1->get_hop(i));
    hops.push_back(hop);
    if (pk1->get_hop(i) == _ip) {
      /* I'm already in this route! */
      return;
    }
  }

  hops.push_back(_ip);

  Vector<u_short> metrics;
  for(int i = 0; i < nhops-2; i++) {
    u_short m = pk1->get_metric(i);
    metrics.push_back(m);
    metric += m;
  }

  metrics.push_back(last_hop_metric);

  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq){
      _seen[si]._when = get_timeval();
      seen_before = true;
      if(metric < _seen[si]._metric){
        // OK, pass this new better route.
        _seen[si]._metrics = metrics;
	_seen[si]._nhops = nhops;
	_seen[si]._hops = hops;
	already_forwarded = _seen[si]._forwarded;
        better = true;
      } 
      break;
    }
  }

  if(_seen.size() > MaxSeen) {
     return;
  }
  if(nhops > MaxHops) {
    return;
  }
  if (seen_before && !better) {
    return;
  }

  /* update the link stats */
  timeval now = get_timeval();
  for(int i = 0; i < nhops-1; i++) {
    IPAddress a = hops[i];
    IPAddress b = hops[i+1];
    u_short m = metrics[i];
    _link_table->update_link(IPPair(a,b), m, now);
  }
  _link_table->dijkstra(_ip);



  if (!seen_before) {
    int delay_time = (last_hop_metric- 100)/10;
    //click_chatter("SRCR %s: setting timer in %d milliseconds", _ip.s().cc(), delay_time);
    srcr_assert(delay_time > 0);

    Seen s = Seen(src, dst, seq, get_timeval(), metric, metrics, hops, nhops);
    _seen.push_back(s);

    Timer *t = new Timer(static_query_hook, (void *) this);
    t->initialize(this);
    
    t->schedule_after_ms(delay_time);
  } else if (already_forwarded) {

    //click_chatter("SRCR %s: forwarding immediately", _ip.s().cc());
    forward_query(Seen(src, dst, seq, get_timeval(), metric, metrics, hops, nhops));
  }
  
  //click_chatter("SRCR %s: finished process_query\n", _ip.s().cc());
}
void
SRCR::forward_query(Seen s)
{
  u_short nhops = s._nhops;


  int len = sr_pkt::len_wo_data(nhops);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);

  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = s._dst;
  pk->_seq = htonl(s._seq);
  pk->_nhops = htons(nhops);

  for(int i=0; i < nhops; i++) {
    pk->set_hop(i, s._hops[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_metric(i, s._metrics[i]);
  }

  if(s._forwarded) {
    pk->_flags |= PF_BETTER;
  }

  s._forwarded = true;

  send(p);
}

// Continue unicasting a reply packet.
void
SRCR::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  u_short next = ntohs(pk1->_next);
  u_short nhops = ntohs(pk1->_nhops);
  
  srcr_assert(type == PT_REPLY);
  next = next - 1;

  if(next >= nhops) {
    click_chatter("SRCR %s: forward_reply strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  next, 
		  nhops);
    return;
  }

  /* update the link stats */
  timeval now = get_timeval();
  for(int i = next - 1; i > 0; i++) {
    IPAddress a = pk1->get_hop(i);
    IPAddress b = pk1->get_hop(i-1);
    u_short m = pk1->get_metric(i);
    _link_table->update_link(IPPair(a,b), m, now);
  }
  _link_table->dijkstra(_ip);

  int len = pk1->hlen_wo_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memcpy(pk, pk1, len);

  for(int i = 0; i < nhops; i++) {
    pk->set_metric(i, pk1->get_metric(i));
  }
  pk->_next = htons(next);

  send(p);

}

// Continue unicasting a data packet.
void
SRCR::forward_data(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  u_short next = ntohs(pk1->_next);
  u_short nhops = ntohs(pk1->_nhops);
  
  /* add the last hop's data onto the metric */
  u_short last_hop_metric = get_metric(pk1->get_hop(next - 1));

  srcr_assert(type == PT_DATA);
  
  next = next + 1;

  if(next >= nhops) {
    click_chatter("SRCR %s: forward_data strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  next, 
		  nhops);
    
    return;
  }

  /* update the link stats */
  timeval now = get_timeval();
  for(int i = 0; i < next; i++) {
    IPAddress a = pk1->get_hop(i);
    IPAddress b = pk1->get_hop(i+1);
    u_short m = pk1->get_metric(i);
    _link_table->update_link(IPPair(a,b), m, now);
  }
  _link_table->dijkstra(_ip);


  int len = pk1->hlen_with_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memcpy(pk, pk1, len);

  for(int i = 0; i < nhops - 1; i++) {
    pk->set_metric(i, pk1->get_metric(i));
  }
  pk->set_metric(next - 1, last_hop_metric);

  pk->_next = htons(next);
  send(p);

}

String
SRCR::Seen::s()
{
  StringAccum sa;
  sa << "[m="<< _metric << " ";
  sa << "nhops=" << _nhops << " ";
  for (int i= 0; i < _nhops; i++) {
    sa << _hops[i].s().cc() << " ";
    if(i + 1 < _nhops) {
      sa << " ";
    }
  }
  sa << "]";

  return sa.take_string();
}

void
SRCR::start_reply(struct sr_pkt *pk1)
{
  u_short nhops = ntohs(pk1->_nhops);
  if(nhops > MaxHops)
    return;
  int len = sr_pkt::len_wo_data(nhops + 1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  
  memcpy(pk, pk1, len);
  pk->_type = PT_REPLY;
  pk->_nhops = htons(nhops + 1);
  pk->set_hop(nhops, _ip.in_addr());

  for(int i = 0; i < nhops - 1; i++) {
    pk->set_metric(i, pk1->get_metric(i));
  }
  pk->set_metric(nhops - 1, get_metric(IPAddress(pk->get_hop(nhops-2))));
  pk->_next = htons(nhops - 1); // Indicates next hop.

  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
SRCR::got_reply(struct sr_pkt *pk)
{
  int di = find_dst(pk->_qdst, false);
  if(di < 0){
    click_chatter("SRCR %s: reply but no Dst %s",
                  _ip.s().cc(),
                  IPAddress(pk->_qdst).s().cc());
    return;
  }
  Dst &dst = _dsts[di];
  if(ntohl(pk->_seq) != dst._seq){
    click_chatter("SRCR %s: reply but wrong seq %d %d",
                  _ip.s().cc(),
                  ntohl(pk->_seq),
                  dst._seq);
    return;
  }

  /* update the route metrics */
  timeval now = get_timeval();
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IPAddress a = pk->get_hop(i);
    IPAddress b = pk->get_hop(i+1);
    u_short m = pk->get_metric(i);
    _link_table->update_link(IPPair(a,b), m, now);
  }
  _link_table->dijkstra(_ip);

}

// Process a packet from the net, sent by a different SRCR.
void
SRCR::got_sr_pkt(Packet *p_in)
{
  struct sr_pkt *pk = (struct sr_pkt *) p_in->data();
  //click_chatter("SRCR %s: got sr packet", _ip.s().cc());
  if(p_in->length() < 20 || p_in->length() < pk->hlen_wo_data()){
    click_chatter("SRCR %s: bad sr_pkt len %d, expected %d",
                  _ip.s().cc(),
                  p_in->length(),
		  pk->hlen_wo_data());
    return;
  }
  if(pk->ether_type != ETHERTYPE_SRCR){
    click_chatter("SRCR %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(pk->ether_type));
    return;
  }

  u_char type = pk->_type;
  u_short nhops = ntohs(pk->_nhops);
  u_short next = ntohs(pk->_next);

  if(type == PT_QUERY && nhops >= 1){
    got_arp(IPAddress(pk->get_hop(nhops-1)), pk->ether_shost);
    if(pk->_qdst == _ip.in_addr()){
      start_reply(pk);
    } else {
      process_query(pk);
    }
  } else if(type == PT_REPLY && next < nhops){
    if(pk->get_hop(next) != _ip.in_addr()){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("SRCR %s: reply not for me %d/%d %s",
                    _ip.s().cc(),
                    ntohs(pk->_next),
                    ntohs(pk->_nhops),
                    IPAddress(pk->get_hop(next)).s().cc());
      return;
    }
    if(next + 1 == nhops)
      got_arp(IPAddress(pk->_qdst), pk->ether_shost);
    else
      got_arp(IPAddress(pk->get_hop(next+1)), pk->ether_shost);
    if(next == 0){
      // I'm the ultimate consumer of this reply. Add to routing tbl.
      got_reply(pk);
    } else {
      // Forward the reply.
      forward_reply(pk);
    }
  } else if(type == PT_DATA && next < nhops){
    if(pk->get_hop(next) != _ip.in_addr()){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("SRCR %s: data not for me %d/%d %s",
                    _ip.s().cc(),
                    ntohs(pk->_next),
                    ntohs(pk->_nhops),
                    IPAddress(pk->get_hop(next)).s().cc());
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
    click_chatter("SRCR %s: bad sr_pkt type=%x",
                  _ip.s().cc(),
                  type);
  }

  return;
}


String 
SRCR::route_to_string(Vector<IPAddress> s) 
{
  StringAccum sa;
  sa << "[ ";
  for (int i=0; i<s.size(); i++) {
    sa << s[i] << " ";
  }
  sa << "]";
  return sa.take_string();
}
void
SRCR::push(int port, Packet *p_in)
{
  if(port == 0){
    // Packet from upper layers in same host.
    Vector<IPAddress> r = _link_table->best_route(p_in->dst_ip_anno());
    if(r.size() > 1)
      start_data(p_in->data(), p_in->length(), r);
    else
      start_query(p_in->dst_ip_anno());
  } else {
    got_sr_pkt(p_in);
  }
  p_in->kill();
}


void
SRCR::query_hook(Timer *t) 
{
  srcr_assert(t);

  Vector<Seen> v;
  timeval now = get_timeval();
  for (int i = 0; i < _seen.size(); i++) {
    if (_seen[i]._when.tv_sec != 0) {
      if (timeval_past(add_millisec(_seen[i]._when, QueryLife), now)) {
	forward_query(_seen[i]);
      } else {
	v.push_back(_seen[i]);
      }
    } 
  }
  _seen = v;


}

String
SRCR::static_print_stats(Element *f, void *)
{
  SRCR *d = (SRCR *) f;
  return d->print_stats();
}

String
SRCR::print_stats()
{
  
  return
    String(_queries) + " queries sent\n" +
    String(_querybytes) + " bytes of query sent\n" +
    String(_replies) + " replies sent\n" +
    String(_replybytes) + " bytes of reply sent\n" +
    String(_datas) + " datas sent\n" +
    String(_databytes) + " bytes of data sent\n";

}

int
SRCR::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SRCR *n = (SRCR *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`frozen' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
SRCR::clear() 
{
  _link_table->clear();
  _dsts.clear();
  _seen.clear();

  _queries = 0;
  _querybytes = 0;
  _replies = 0;
  _replybytes = 0;
  _datas = 0;
  _databytes = 0;
}

void
SRCR::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
}

 void
SRCR::srcr_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SRCR %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

// generate Vector template instance
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SRCR::Dst>;
template class Vector<SRCR::IPAddresss>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRCR)

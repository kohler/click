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
   *  These are in ms
   */
  QueryInterval = 10000;
  QueryLife = 3000; 
  ARPLife = 30000;

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
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
  unsigned int now = click_jiffies();
  Vector<Seen> v;
  int i;
  for(i = 0; i < _seen.size(); i++)
    if(ms_to_jiff(QueryLife) + _seen[i]._when > now) {
      v.push_back(_seen[i]);
    }
  _seen = v;

  ARPTable a;
  for (ARPTable::iterator iter = _arp.begin(); iter; iter++) {
    ARP arp = iter.value();
    if (arp._when + ms_to_jiff(ARPLife) >= now) {
      a.insert(arp._ip, arp);
    }
  }
  _arp = a;
  _timer.schedule_after_ms(1000);
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
  pk->_next = htons(1);
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
  /* now hand the packet to the upper layers */
  Packet *p = Packet::make(pk->data(), pk->_dlen);
  
  output(0).push(p);
}

void
SRCR::start_query(IPAddress dstip)
{
  Dst *dst = _dsts.findp(dstip);
  if (!dst) {
    _dsts.insert(dstip, Dst(dstip));
    dst = _dsts.findp(dstip);
  }
  srcr_assert(dst);
  dst->_best_metric = 9999;
  unsigned int now = click_jiffies();
  if (dst->_when != 0 && now < dst->_when + ms_to_jiff(QueryInterval)) {
    // We sent a query less than 10 seconds ago, don't repeat.
    return;
  }

  dst->_seq = ++_seq;

  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = dst->_ip;
  pk->_seq = htonl(dst->_seq);
  pk->_nhops = htons(1);
  pk->set_hop(0,_ip.in_addr());
  
  dst->_when = now;

  Vector<IPAddress> hops;
  Vector<u_short> metrics;
  hops.push_back(IPAddress(_ip));
  _seen.push_back(Seen(_ip.in_addr(), dstip, pk->_seq, now, 0, 0, metrics, hops, 1));

  send(p);
}


// A packet has arrived from ip/en.
// Enter the pair in our ARP table.
void
SRCR::got_arp(IPAddress ip, EtherAddress en)
{
  _arp.insert(ip, ARP(ip, en, click_jiffies()));
}

// Given an IP address, look in our local ARP table to see
// if we happen to know that node's ethernet address.
// If we don't, just use ff:ff:ff:ff:ff:ff
EtherAddress
SRCR::find_arp(IPAddress ip)
{

  ARP *a = _arp.findp(ip);

  if (a) {
    return a->_en;
  } else {
    click_chatter("SRCR %s: find_arp %s ???",
		  _ip.s().cc(),
		  ip.s().cc());
    return _bcast;
  }
}

// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
SRCR::send(WritablePacket *p)
{
  struct sr_pkt *pk = (struct sr_pkt *) p->data();

  pk->ether_type = htons(ETHERTYPE_SRCR);
  memcpy(pk->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    //memcpy(pk->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
    memcpy(pk->ether_dhost, _bcast.data(), 6);
    _queries++;
    _querybytes += p->length();
  } else if(type == PT_REPLY || type == PT_DATA){
    u_short next = ntohs(pk->_next);
    srcr_assert(next < MaxHops + 2);
    struct in_addr nxt = pk->get_hop(next);
    EtherAddress eth_dest = find_arp(IPAddress(nxt));
    memcpy(pk->ether_dhost, eth_dest.data(), 6);
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
  u_short dft = 9999; // default metric
  if(_link_stat){
    unsigned int tau;
    struct timeval tv;
    unsigned int frate, rrate;
    bool res = _link_stat->get_forward_rate(other, &frate, &tau, &tv);
    if(res == false) {
      return dft;
    }
    res = _link_stat->get_reverse_rate(other, &rrate, &tau);
    if(res == false) {
      return dft;
    }
    if(frate == 0 || rrate == 0) {
      return dft;
    }
    u_short m = 100 * 100 * 100 / (frate * (int) rrate);
    return m;
  } else {
    click_chatter("no link stat!!!!");
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
  u_short last_hop_metric = get_metric(IPAddress(pk1->get_hop(pk1->num_hops()-1)));
  u_short metric = 0;
  u_long seq = ntohl(pk1->_seq);
  int si;
  bool better = false;
  bool seen_before = false;
  bool already_forwarded = false;

  Vector<IPAddress> hops;
  for(int i = 0; i < pk1->num_hops(); i++) {
    IPAddress hop = IPAddress(pk1->get_hop(i));
    hops.push_back(hop);
    if (pk1->get_hop(i) == _ip) {
      /* I'm already in this route! */
      return;
    }
  }

  hops.push_back(_ip);

  Vector<u_short> metrics;
  for(int i = 0; i < pk1->num_hops()-1; i++) {
    u_short m = pk1->get_metric(i);
    metrics.push_back(m);
    metric += m;
  }

  metrics.push_back(last_hop_metric);

  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq){
      _seen[si]._when = click_jiffies();
      seen_before = true;
      if(metric < _seen[si]._metric){
        // OK, pass this new better route.
        _seen[si]._metrics = metrics;
	_seen[si]._nhops = pk1->num_hops()+1;
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
  if(pk1->num_hops() > MaxHops) {
    return;
  }

  if (seen_before && !better) {
    return;
  }
  unsigned int now = click_jiffies();
  if (!seen_before) {
    /* add one so perfect links delay some */
    int delay_time = (last_hop_metric-100)/10 + 1;
    click_chatter("SRCR %s: setting timer in %d milliseconds", _ip.s().cc(), delay_time);
    srcr_assert(delay_time > 0);

    Seen s = Seen(src, dst, seq, now, ms_to_jiff(delay_time), metric, metrics, hops, pk1->num_hops()+1);
    _seen.push_back(s);

    Timer *t = new Timer(static_query_hook, (void *) this);
    t->initialize(this);
    
    t->schedule_after_ms(delay_time);
  } else if (already_forwarded) {

    //click_chatter("SRCR %s: forwarding immediately", _ip.s().cc());
    forward_query(Seen(src, dst, seq, now, 0, metric, metrics, hops, pk1->num_hops()+1));
  }
  
  //click_chatter("SRCR %s: finished process_query\n", _ip.s().cc());
}
void
SRCR::forward_query(Seen s)
{
  u_short nhops = s._nhops;

  click_chatter("forward query called");
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
  
  srcr_assert(type == PT_REPLY);

  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("SRCR %s: forward_reply strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  pk1->next(), 
		  pk1->num_hops());
    return;
  }

  int len = pk1->hlen_wo_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memcpy(pk, pk1, len);

  pk->set_next(pk1->next() - 1);

  send(p);

}

// Continue unicasting a data packet.
void
SRCR::forward_data(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  
  /* add the last hop's data onto the metric */
  u_short last_hop_metric = get_metric(IPAddress(pk1->get_hop(pk1->next() - 1)));

  srcr_assert(type == PT_DATA);
  
  if(pk1->next() + 1 >= pk1->num_hops()) {
    click_chatter("SRCR %s: forward_data strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  pk1->next() + 1,
		  pk1->num_hops());
    
    return;
  }

  int len = pk1->hlen_with_data();
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memcpy(pk, pk1, len);

  
  u_short next_hop_metric = get_metric(IPAddress(pk1->get_hop(pk1->next()+1)));
  click_chatter("SRCR %s: next hop metric = %d", _ip.s().cc(), next_hop_metric);
  if (next_hop_metric >= 9999) {
    /* reply with a error */
    start_error(pk1);
    return;
  }
  pk->set_metric(pk1->next() - 1, last_hop_metric);
  pk->set_next(pk1->next() + 1);

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
SRCR::start_error(struct sr_pkt *pk1)
{
  int len = sr_pkt::len_wo_data(pk1->next()+2);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  
  memcpy(pk, pk1, len);

  IPAddress neighbor = IPAddress(pk1->get_hop(pk1->next()+1));
  u_short m = get_metric(neighbor);
  pk->_type = PT_REPLY;

  pk->set_num_hops(pk1->next()+2);
  pk->set_hop(pk1->next()+1, neighbor);
  
  for (int i=0; i < pk1->next(); i++) {
    pk->set_metric(i, pk1->get_metric(i));
  }
  pk->set_metric(pk1->next(), m);
  pk->set_next(pk1->next() - 1); // indicates next hop

  send(p);


}
void
SRCR::start_reply(struct sr_pkt *pk1)
{
  if(pk1->num_hops() > MaxHops)
    return;

  int len = sr_pkt::len_wo_data(pk1->num_hops() + 1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  
  memcpy(pk, pk1, len);

  IPAddress neighbor = IPAddress(pk1->get_hop(pk1->num_hops()-1));
  u_short m = get_metric(neighbor);
  pk->_type = PT_REPLY;
  pk->set_num_hops(pk1->num_hops() + 1);
  pk->set_hop(pk1->num_hops(), _ip.in_addr());
  
  for (int i=0; i < pk1->num_hops()-1; i++) {
    pk->set_metric(i, pk1->get_metric(i));
  }
  pk->set_metric(pk1->num_hops() - 1, m);
  pk->set_next(pk1->num_hops() - 1); // indicates next hop

  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
SRCR::got_reply(struct sr_pkt *pk)
{
  Dst *dst = _dsts.findp(IPAddress(pk->_qdst));
  if(!dst){
    click_chatter("SRCR %s: reply but no Dst %s",
                  _ip.s().cc(),
                  IPAddress(pk->_qdst).s().cc());
    return;
  }
  if(ntohl(pk->_seq) != dst->_seq){
    click_chatter("SRCR %s: reply but wrong seq %d %d",
                  _ip.s().cc(),
                  ntohl(pk->_seq),
                  dst->_seq);
    return;
  }

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
  if(pk->ether_type != htons(ETHERTYPE_SRCR)){
    click_chatter("SRCR %s: bad ether_type %04x",
                  _ip.s().cc(),
                  ntohs(pk->ether_type));
    return;
  }

  u_char type = pk->_type;
  u_short nhops = ntohs(pk->_nhops);
  u_short next = ntohs(pk->_next);


  /* update the metrics from the packet */
  unsigned int now = click_jiffies();
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IPAddress a = pk->get_hop(i);
    IPAddress b = pk->get_hop(i+1);
    u_short m = pk->get_metric(i);
    if (m != 0) {
      //click_chatter("updating %s <%d> %s", a.s().cc(), m, b.s().cc());
      update_link(IPPair(a,b), m, now);
    }
  }
  
  IPAddress neighbor = IPAddress(0);
  switch (type) {
  case PT_QUERY:
    neighbor = IPAddress(pk->get_hop(pk->num_hops() - 1));
    break;
  case PT_REPLY:
    neighbor = IPAddress(pk->get_hop(pk->next()+1));
    break;
  case PT_DATA:
    neighbor = IPAddress(pk->get_hop(pk->next()-1));
    break;
  default:
    srcr_assert(0);
  }
  u_short m = get_metric(neighbor);
  //click_chatter("updating %s <%d> %s", neighbor.s().cc(), m,  _ip.s().cc());
  update_link(IPPair(neighbor, _ip), m, now);
  update_best_metrics();

  got_arp(neighbor, EtherAddress(pk->ether_shost));

  if(type == PT_QUERY && nhops >= 1){

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
    if(next == nhops -1){
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
    IPAddress p = p_in->dst_ip_anno();
    Dst *dst = _dsts.findp(p);
    
    // Packet from upper layers in same host.
    u_short current_metric = _link_table->get_host_metric(p);
    if (dst && current_metric != 9999 && dst->_best_metric/2 > current_metric ) {
      Vector<IPAddress> r = _link_table->best_route(p);
      srcr_assert(r.size() > 1);
      //click_chatter("found route for %s: %s\n", p.s().cc(), 
      //route_to_string(_link_table->best_route(p).cc());
      start_data(p_in->data(), p_in->length(), r);
    } else {
      //click_chatter("startin query for %s\n", p.s().cc());
      start_query(p);
    }
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
  unsigned int now = click_jiffies();
  for (int i = 0; i < _seen.size(); i++) {
    if (_seen[i]._when != 0) {
      if (now >= _seen[i]._when + _seen[i]._delay) {
	forward_query(_seen[i]);
      } else {
	v.push_back(_seen[i]);
      }
    } 
  }
  _seen = v;


}

String
SRCR::static_print_arp(Element *f, void *)
{
  SRCR *d = (SRCR *) f;
  return d->print_arp();
}
String
SRCR::print_arp()
{
  StringAccum sa;
  for (ARPTable::iterator iter = _arp.begin(); iter; iter++) {
    ARP arp = iter.value();
    sa << "arp:" << arp._ip.s().cc() << " " << arp._en.s().cc() << "\n";
  }
  return sa.take_string();

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
  add_read_handler("arp", static_print_arp, 0);
}
void
SRCR::update_link(IPPair p, u_short m, unsigned int now)
{
  _link_table->update_link(p, m, now);
}
void
SRCR::update_best_metrics() 
{
  _link_table->dijkstra(_ip);
  /* 
   * it should be the case that all dsts in the linktable
   * have a corresponding Dst in _dsts 
   */
  Vector<IPAddress> v = _link_table->get_hosts();
  for (int i=0; i < v.size(); i++) {
    Dst *dst = _dsts.findp(v[i]);
    if (!dst) {
      _dsts.insert(v[i], Dst(v[i]));
      dst = _dsts.findp(v[i]);
    }
    srcr_assert(dst);
    
    u_short lt_metric = _link_table->get_host_metric(dst->_ip);
    if (dst->_best_metric < lt_metric) {
      dst->_best_metric = lt_metric;
    }
    
  }
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
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SRCR::IPAddresss>;
template class HashMap<IPAddress, SRCR::ARP>;
template class HashMap<IPAddress, SRCR::Dst>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRCR)

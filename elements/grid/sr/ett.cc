/*
 * ETT.{cc,hh} -- DSR implementation
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
#include "ett.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef ett_assert
#define ett_assert(e) ((e) ? (void) 0 : ett_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


ETT::ETT()
  :  Element(3,2),
     _timer(this), 
     _warmup(0),
     _metric(0),
     _arp_table(0),
     _num_queries(0),
     _bytes_queries(0),
     _num_replies(0), 
     _bytes_replies(0)

{
  MOD_INC_USE_COUNT;

  MaxSeen = 200;
  MaxHops = 30;

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

  _query_wait.tv_sec = 5;
  _query_wait.tv_usec = 0;


  _black_list_timeout.tv_sec = 60;
  _black_list_timeout.tv_usec = 0;

  _rev_path_update.tv_sec = 10;
  _rev_path_update.tv_usec = 0;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

ETT::~ETT()
{
  MOD_DEC_USE_COUNT;
}

int
ETT::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _warmup_period = 25;
  ret = cp_va_parse(conf, this, errh,
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIPAddress, "IP address", &_ip,
		    cpEtherAddress, "EtherAddress", &_en,
		    cpElement, "SRCR element", &_srcr,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
		    "METRIC", cpElement, "METRIC element", &_metric,
		    "WARMUP", cpUnsigned, "Warmup period", &_warmup_period,
                    0);

  if (_srcr && _srcr->cast("SRCR") == 0) 
    return errh->error("SRCR element is not a SRCR");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_metric && _metric->cast("GridGenericMetric") == 0) 
    return errh->error("METRIC element is not a GridGenericMetric");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  return ret;
}

ETT *
ETT::clone () const
{
  return new ETT;
}

int
ETT::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
ETT::run_timer ()
{
  if (_warmup <= _warmup_period) {
    _warmup++;
    if (_warmup > _warmup_period) {
      click_chatter("ETT %s: warmup finished\n",
		    id().cc());
    }
  }
  _timer.schedule_after_ms(1000);
}

IPAddress
ETT::get_random_neighbor()
{
  if (!_neighbors_v.size()) {
    return IPAddress();
  }
  int ndx = random() % _neighbors_v.size();
  return _neighbors_v[ndx];

}
void
ETT::start_query(IPAddress dstip)
{
  ett_assert(dstip);
  Query *q = _queries.findp(dstip);
  if (!q) {
    Query foo = Query(dstip);
    _queries.insert(dstip, foo);
    q = _queries.findp(dstip);
  }

  q->_seq = _seq;
  click_gettimeofday(&q->_last_query);
  q->_count++;
  q->_metric = 0;
  click_chatter("ETT %s: start_query %s ->  %s", 
		id().cc(),
		_ip.s().cc(),
		dstip.s().cc());

  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _srcr_version;
  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = dstip;
  pk->_seq = htonl(++_seq);
  pk->set_num_hops(1);
  pk->set_hop(0,_ip);
  send(p);
}



// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
ETT::send(WritablePacket *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);

  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    memset(eh->ether_dhost, 0xff, 6);
    _num_queries++;
    _bytes_queries += p->length();
  } else if(type == PT_REPLY){
    int next = pk->next();
    ett_assert(next < MaxHops);
    EtherAddress eth_dest = _arp_table->lookup(pk->get_hop(next));
    memcpy(eh->ether_dhost, eth_dest.data(), 6);
    _num_replies++;
    _bytes_replies += p->length();
  } else {
    ett_assert(0);
    return;
  }

  output(0).push(p);
}


int
ETT::get_metric(IPAddress other)
{
  ett_assert(other);
  BadNeighbor *n = _black_list.findp(other);
  int metric = 9999;
  if (n && n->still_bad() ) {
    metric = 9999;
  } else if (_metric && _arp_table) {
    EtherAddress neighbor = _arp_table->lookup(other);
    ett_assert(neighbor);
    GridGenericMetric::metric_t t = _metric->get_link_metric(neighbor);
    if (t.good()) {
      metric = t.val();
    } else {
      metric = 9999;
    }
  }
  update_link(_ip, other, metric);
  return metric;
}

void
ETT::update_link(IPAddress from, IPAddress to, int metric) {
  _link_table->update_link(from, to, metric);
  _link_table->update_link(to, from, metric);
}

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
ETT::process_query(struct sr_pkt *pk1)
{
  IPAddress src(pk1->get_hop(0));
  IPAddress dst(pk1->_qdst);
  u_long seq = ntohl(pk1->_seq);
  int si;

  Vector<IPAddress> hops;
  Vector<int> metrics;

  int metric = 0;
  for(int i = 0; i < pk1->num_hops(); i++) {
    IPAddress hop = IPAddress(pk1->get_hop(i));
    if (i != pk1->num_hops() - 1) {
      metrics.push_back(pk1->get_metric(i));
      metric += pk1->get_metric(i);
    }
    hops.push_back(hop);
    if (hop == _ip) {
      /* I'm already in this route! */
      return;
    }
  }
  _link_table->dijkstra();
  /* also get the metric from the neighbor */
  int m = get_metric(pk1->get_hop(pk1->num_hops()-1));
  update_link(_ip, pk1->get_hop(pk1->num_hops()-1), m);
  metric += m;
  metrics.push_back(m);
  hops.push_back(_ip);

  
  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq){
      break;
    }
  }

  if (si == _seen.size()) {
    if (_seen.size() == 100) {
      _seen.pop_front();
      si--;
    }
    _seen.push_back(Seen(src, dst, seq, 0));
  }
  _seen[si]._count++;
  if (_seen[si]._metric && _seen[si]._metric <= metric) {
    /* the metric is worse that what we've seen*/
    return;
  }

  _seen[si]._metric = metric;

  click_gettimeofday(&_seen[si]._when);
  
  if (dst == _ip) {
    /* query for me */
    start_reply(pk1);
  } else {
    _seen[si]._hops = hops;
    _seen[si]._metrics = metrics;
    if (timercmp(&_seen[si]._when, &_seen[si]._to_send, <)) {
      /* a timer has already been scheduled */
      return;
    } else {
      /* schedule timer */
      int delay_time = random() % 750 + 1;
      ett_assert(delay_time > 0);

      struct timeval delay;
      delay.tv_sec = 0;
      delay.tv_usec = delay_time*1000;
      timeradd(&_seen[si]._when, &delay, &_seen[si]._to_send);
      _seen[si]._forwarded = false;
      Timer *t = new Timer(static_forward_query_hook, (void *) this);
      t->initialize(this);
      
      t->schedule_at(_seen[si]._to_send);
    }
  }

}
void
ETT::forward_query_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _seen.size(); x++) {
    if (timercmp(&_seen[x]._to_send, &now, <) && !_seen[x]._forwarded) {
      forward_query(&_seen[x]);
    }
  }
}
void
ETT::forward_query(Seen *s)
{
  click_chatter("ETT %s: forward_query %s -> %s\n", 
		id().cc(),
		s->_src.s().cc(),
		s->_dst.s().cc());
  int nhops = s->_hops.size();

  ett_assert(s->_hops.size() == s->_metrics.size()+1);

  //click_chatter("forward query called");
  int len = sr_pkt::len_wo_data(nhops);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _srcr_version;
  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = s->_dst;
  pk->_seq = htonl(s->_seq);
  pk->set_num_hops(nhops);

  for(int i=0; i < nhops; i++) {
    pk->set_hop(i, s->_hops[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_metric(i, s->_metrics[i]);
  }

  s->_forwarded = true;
  send(p);
}

// Continue unicasting a reply packet.
void
ETT::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  ett_assert(type == PT_REPLY);

  _link_table->dijkstra();
  click_chatter("ETT %s: forward_reply %s <- %s\n", 
		id().cc(),
		pk1->get_hop(0).s().cc(),
		IPAddress(pk1->_qdst).s().cc());
  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("ETT %s: forward_reply strange next=%d, nhops=%d", 
		  _ip.s().cc(), 
		  pk1->next(), 
		  pk1->num_hops());
    return;
  }

  Path fwd;
  Path rev;
  for (int i = 0; i < pk1->num_hops(); i++) {
    fwd.push_back(pk1->get_hop(i));
  }
  rev = reverse_path(fwd);
  struct timeval now;
  click_gettimeofday(&now);

  PathInfo *fwd_info = _paths.findp(fwd);
  if (!fwd_info) {
    _paths.insert(fwd, PathInfo(fwd));
    fwd_info = _paths.findp(fwd);
  }
  PathInfo *rev_info = _paths.findp(rev);
  if (!rev_info) {
    _paths.insert(rev, PathInfo(rev));
    rev_info = _paths.findp(rev);
  }
  fwd_info->_last_packet = now;
  rev_info->_last_packet = now;


  int len = pk1->hlen_wo_data();
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memcpy(pk, pk1, len);

  pk->set_next(pk1->next() - 1);

  send(p);

}

void ETT::start_reply(struct sr_pkt *pk_in)
{

  int len = sr_pkt::len_wo_data(pk_in->num_hops()+1);
  _link_table->dijkstra();
  click_chatter("ETT %s: start_reply %s <- %s\n",
		id().cc(),
		pk_in->get_hop(0).s().cc(),
		IPAddress(pk_in->_qdst).s().cc());
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk_out = (struct sr_pkt *) (eh+1);
  memset(pk_out, '\0', len);


  pk_out->_version = _srcr_version;
  pk_out->_type = PT_REPLY;
  pk_out->_flags = 0;
  pk_out->_seq = pk_in->_seq;
  pk_out->set_num_hops(pk_in->num_hops()+1);
  pk_out->set_next(pk_in->num_hops() - 1);
  pk_out->_qdst = pk_in->_qdst;


  for (int x = 0; x < pk_in->num_hops(); x++) {
    IPAddress hop = pk_in->get_hop(x);
    pk_out->set_hop(x, hop);
    if (x < pk_in->num_hops() - 1) {
      int m = pk_in->get_metric(x);
      pk_out->set_metric(x, m);
    }
  }
  IPAddress prev = pk_in->get_hop(pk_in->num_hops()-1);
  int last_hop_m = get_metric(prev);
  pk_out->set_hop(pk_in->num_hops(), _ip);
  pk_out->set_metric(pk_in->num_hops()-1, last_hop_m);

  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
ETT::got_reply(struct sr_pkt *pk)
{

  IPAddress dst = IPAddress(pk->_qdst);
  ett_assert(dst);
  click_chatter("ETT %s: got_reply %s <- %s\n", 
		id().cc(),
		_ip.s().cc(),
		dst.s().cc());
  _link_table->dijkstra();
  
  Path p;
  for (int i = 0; i < pk->num_hops(); i++) {
    p.push_back(pk->get_hop(i));
  }
  int metric = _link_table->get_route_metric(p);
  Query *q = _queries.findp(dst);
  if (!q) {
    Query foo = Query(dst);
    _queries.insert(dst, foo);
    q = _queries.findp(dst);
  }
  ett_assert(q);
  q->_count = 0;
  if ((!q->_metric || q->_metric > metric) && metric < 9999) {
    q->_metric = metric;
  }
}


void
ETT::process_data(Packet *p_in)
{
  Path fwd;
  Path rev;
  click_ether *eh_in = (click_ether *) p_in->data();
  struct sr_pkt *pk_in = (struct sr_pkt *) (eh_in+1);
  
  IPAddress dst = pk_in->get_hop(pk_in->next());

  for (int x = 0; x < pk_in->num_hops(); x++) {
    fwd.push_back(pk_in->get_hop(x));
  }

  int i = 0;
  for (i = 0; i < fwd.size(); i++) {
    if (fwd[i] == _ip) {
      break;
    }
  }
  int ndx_me = i;
  ett_assert(ndx_me != fwd.size());
  ett_assert(ndx_me != 0);
  if (ndx_me == fwd.size()-1) {
    /* I'm the last hop */
    return;
  }


  struct timeval now;
  click_gettimeofday(&now);

  PathInfo *fwd_info = _paths.findp(fwd);
  if (!fwd_info) {
    _paths.insert(fwd, PathInfo(fwd));
    fwd_info = _paths.findp(fwd);
  }
  fwd_info->_last_packet = now;

  rev = reverse_path(fwd);
  PathInfo *rev_info = _paths.findp(rev);
  if (!rev_info) {
    _paths.insert(rev, PathInfo(rev));
    rev_info = _paths.findp(rev);
    rev_info->_last_packet = now;
  }
  
  struct timeval expire;
  timeradd(&rev_info->_last_packet, &_rev_path_update, &expire);
  if (!timercmp(&expire, &now, <)) {
    return;
  }
  
  if (random() % (fwd.size() - ndx_me + 1) != 0) {
    return;
  }

  /* send an update */

  rev_info->_last_packet = now;

  int len = sr_pkt::len_wo_data(fwd.size());
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0) {
    click_chatter("ETT %s: couldn't make packet in process_data\n",
		  id().cc());
    return;
  }
  
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _srcr_version;
  pk->_type = PT_REPLY;
  pk->_flags = FLAG_UPDATE;
  pk->_qdst = dst;
  pk->_seq = 0;
  pk->set_next(ndx_me-1);
  pk->set_num_hops(fwd.size());
  for(i = 0; i < fwd.size(); i++) {
    pk->set_hop(i, fwd[i]);
    if (i < ndx_me) {
      pk->set_metric(i, _link_table->get_hop_metric(fwd[i],fwd[i+1]));
    }
  }
  
  pk->set_metric(ndx_me, get_metric(fwd[ndx_me+1]));

  send(p);
  return;

}
void
ETT::push(int port, Packet *p_in)
{
  if (_warmup < _warmup_period) {
    p_in->kill();
    return;
  }

  if (port == 1) {
    process_data(p_in);
    p_in->kill();
    return;
  }
  if (port == 2) {
    bool sent_packet = false;
    IPAddress dst = p_in->dst_ip_anno();
    
    ett_assert(dst);
    Path p = _link_table->best_route(dst);
    int metric = _link_table->get_route_metric(p);

    if (_link_table->valid_route(p)) {
      Packet *p_out = _srcr->encap(p_in->data(), p_in->length(), p);
      if (p_out) {
	sent_packet = true;
	output(1).push(p_out);
      } else {
	click_chatter("%s: couldn't encap new packet!\n",
		      id().cc());
      }
    }

    p_in->kill();

    Query *q = _queries.findp(dst);
    if (!q) {
      Query foo = Query(dst);
      _queries.insert(dst, foo);
      q = _queries.findp(dst);
    }
    ett_assert(q);

    if (q->_metric > metric) {
      q->_metric = metric;
    }
    if (sent_packet && metric < 2 * q->_metric) {
      return;
    }
    
    struct timeval n;
    click_gettimeofday(&n);
    struct timeval expire;
    timeradd(&q->_last_query, &_query_wait, &expire);
    if (timercmp(&expire, &n, <)) {
      start_query(dst);
    }
    return;

    
  } else if (port ==  0) {
    
    click_ether *eh = (click_ether *) p_in->data();
    struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
    if(eh->ether_type != htons(_et)) {
      click_chatter("ETT %s: bad ether_type %04x",
		    _ip.s().cc(),
		    ntohs(eh->ether_type));
      p_in->kill();
      return;
    }
    if (EtherAddress(eh->ether_shost) == _en) {
      click_chatter("ETT %s: packet from me");
      p_in->kill();
      return;
    }
    
    u_char type = pk->_type;
    
    /* update the metrics from the packet */
    for(int i = 0; i < pk->num_hops()-1; i++) {
      IPAddress a = pk->get_hop(i);
      IPAddress b = pk->get_hop(i+1);
      int m = pk->get_metric(i);
      if (m != 0 && a != _ip && b != _ip) {
	/* 
	 * don't update the link for my neighbor
	 * we'll do that below
	 */
	update_link(a,b,m);
      }
    }
    
    
    IPAddress neighbor = IPAddress();
    switch (type) {
    case PT_QUERY:
      neighbor = pk->get_hop(pk->num_hops()-1);
      break;
    case PT_REPLY:
      neighbor = pk->get_hop(pk->next()+1);
      break;
    default:
      ett_assert(0);
    }

    ett_assert(neighbor);
    if (!_neighbors.findp(neighbor)) {
      _neighbors.insert(neighbor, true);
      _neighbors_v.push_back(neighbor);
    }

    _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
    update_link(_ip, neighbor, get_metric(neighbor));
    if(type == PT_QUERY){
      process_query(pk);
      
    } else if(type == PT_REPLY){
      if(pk->get_hop(pk->next()) != _ip){
	// it's not for me. these are supposed to be unicast,
	// so how did this get to me?
	click_chatter("ETT %s: reply not for me %d/%d %s",
		      _ip.s().cc(),
		      pk->next(),
		      pk->num_hops(),
		      pk->get_hop(pk->next()).s().cc());
	p_in->kill();
	return;
      }
      if(pk->next() == 0){
	// I'm the ultimate consumer of this reply. Add to routing tbl.
	got_reply(pk);
      } else {
	// Forward the reply.
	forward_reply(pk);
      }
    } else {
      click_chatter("ETT %s: bad sr_pkt type=%x",
		    _ip.s().cc(),
		    type);
    }
    
  }
  p_in->kill();
  return;
  


}


String
ETT::static_print_stats(Element *f, void *)
{
  ETT *d = (ETT *) f;
  return d->print_stats();
}

String
ETT::print_stats()
{
  
  return
    String(_num_queries) + " queries sent\n" +
    String(_bytes_queries) + " bytes of query sent\n" +
    String(_num_replies) + " replies sent\n" +
    String(_bytes_replies) + " bytes of reply sent\n";
}

int
ETT::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ETT *n = (ETT *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
ETT::clear() 
{
  _link_table->clear();
  _seen.clear();

  _num_queries = 0;
  _bytes_queries = 0;
  _num_replies = 0;
  _bytes_replies = 0;
}

int
ETT::static_start(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ETT *n = (ETT *) e;
  IPAddress dst;

  if (!cp_ip_address(arg, &dst))
    return errh->error("dst must be an IPAddress");

  n->start(dst);
  return 0;
}
void
ETT::start(IPAddress dst) 
{
  click_chatter("write handler called with dst %s", dst.s().cc());
  start_query(dst);
}


int
ETT::static_link_failure(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ETT *n = (ETT *) e;
  EtherAddress dst;
  String foo = arg;
  if (!cp_ethernet_address(arg, &dst))
    return errh->error("ETT link_falure handler: dst not etheraddress");

  n->link_failure(dst);
  return 0;
}
void
ETT::link_failure(EtherAddress dst) 
{
  click_chatter("ETT %s: link_failure called for with dst %s", 
		id().cc(), dst.s().cc());
  IPAddress ip = _arp_table->reverse_lookup(dst);

  if (ip == IPAddress()) {
    click_chatter("ETT %s: reverse arp found no ip\n",
		  id().cc());
    return;
  }
  click_chatter("ETT %s: reverse arp found %s\n",
		id().cc(),
		ip.s().cc());
  update_link(_ip, ip, 9999);

  BadNeighbor b = BadNeighbor(ip);
  click_gettimeofday(&b._when);
  b._timeout.tv_sec = _black_list_timeout.tv_sec;
  b._errors_sent.clear();
  _black_list.insert(ip, b);

  /*
   * run dijkstra so the next time we need a route
   * we're forced to do a query
   */
  _link_table->dijkstra();
}

void
ETT::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("start", static_start, 0);
  add_write_handler("link_failure", static_link_failure, 0);
}

void
ETT::ett_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("ETT %s assertion \"%s\" failed: file %s, line %d",
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
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<ETT::IPAddress>;
template class DEQueue<ETT::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(ETT)

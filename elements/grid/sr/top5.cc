/*
 * Top5.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachuseesrs Institute of Technology
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
#include "top5.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef top5_assert
#define top5_assert(e) ((e) ? (void) 0 : top5_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


Top5::Top5()
  :  Element(3,2),
     _timer(this), 
     _warmup(0),
     _link_stat(0),
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

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

Top5::~Top5()
{
  MOD_DEC_USE_COUNT;
}

int
Top5::configure (Vector<String> &conf, ErrorHandler *errh)
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
		    "LS", cpElement, "LinkStat element", &_link_stat,
		    "WARMUP", cpUnsigned, "Warmup period", &_warmup_period,
                    0);

  if (_srcr && _srcr->cast("SRCR") == 0) 
    return errh->error("SRCR element is not a SRCR");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_link_stat && _link_stat->cast("LinkStat") == 0) 
    return errh->error("Link element is not a LinkStat");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  return ret;
}

Top5 *
Top5::clone () const
{
  return new Top5;
}

int
Top5::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
Top5::run_timer ()
{
  if (_warmup <= _warmup_period) {
    _warmup++;
    if (_warmup > _warmup_period) {
      click_chatter("Top5 %s: warmup finished\n",
		    id().cc());
    }
  }
  _timer.schedule_after_ms(1000);
}

void
Top5::start_query(IPAddress dstip)
{
  click_chatter("Top5 %s: start_query %s ->  %s", 
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
Top5::send(WritablePacket *p)
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
    top5_assert(next < MaxHops);
    EtherAddress eth_dest = _arp_table->lookup(pk->get_hop(next));
    memcpy(eh->ether_dhost, eth_dest.data(), 6);
    _num_replies++;
    _bytes_replies += p->length();
  } else {
    top5_assert(0);
    return;
  }

  output(0).push(p);
}


int
Top5::get_metric(IPAddress other)
{
  int metric = 0;
  if (!_link_stat || !_arp_table) {
    metric = 9999;
  } else {
    unsigned int tau;
    struct timeval tv;
    unsigned int frate, rrate;
    if(!_link_stat->get_forward_rate(_arp_table->lookup(other), 
				     &frate, &tau, &tv)) {
      metric = 9999;
    } else if (!_link_stat->get_reverse_rate(_arp_table->lookup(other), 
					     &rrate, &tau)) {
      metric = 9999;
    } else if (frate == 0 || rrate == 0) {
      metric = 9999;
    } else {
      metric = 100 * 100 * 100 / (frate * (int) rrate);
    } 
  }
  update_link(_ip, other, metric);
  return metric;
}

void
Top5::update_link(IPAddress from, IPAddress to, int metric) {
  _link_table->update_link(from, to, metric);
  _link_table->update_link(to, from, metric);
}

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
Top5::process_query(struct sr_pkt *pk1)
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

  if (dst == _ip) {
    int x = 0;
    for(x = 0; x < _replies.size(); x++){
      if(src == _replies[x]._src && seq == _replies[x]._seq){
	break;
      }
    }

    if (x == _replies.size()) {
      if (_replies.size() == 100) {
	_replies.pop_front();
	x--;
      }
      _replies.push_back(Reply());
      struct timeval delay;
      struct timeval now;
      delay.tv_sec = 10;
      delay.tv_usec = 0;
      click_gettimeofday(&now);
      timeradd(&now, &delay, &_replies[x]._to_send);
      _replies[x]._sent = false;
      Timer *t = new Timer(static_start_reply_hook, (void *) this);
      t->initialize(this);
      t->schedule_at(_replies[x]._to_send);

    }
    return;
  }
  
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
  int better_than = TOP_N;
  if (_seen[si]._queries.size() < TOP_N) {
    better_than = _seen[si]._queries.size();
    _seen[si]._queries.push_back(Query());
  } else {
    for (int x = 0; x < _seen[si]._queries.size(); x++) {
      if (!_seen[si]._queries[x]._metric && _seen[si]._metric > metric) {
	better_than = x;
	_seen[si]._queries[x] = Query();
	break;
      }
    }

  }

  if (better_than == TOP_N) {
    return;
  }

  _seen[si]._queries[better_than]._p = hops;
  _seen[si]._queries[better_than]._src = src;
  _seen[si]._queries[better_than]._dst = dst;
  _seen[si]._queries[better_than]._seq = seq;


  /* schedule timer */
  int delay_time = random() % 750 + 1;
  top5_assert(delay_time > 0);
  
  struct timeval delay;
  struct timeval now;
  delay.tv_sec = 0;
  delay.tv_usec = delay_time*1000;
  click_gettimeofday(&now);
  timeradd(&now, &delay, &_seen[si]._queries[better_than]._to_send);
  _seen[si]._queries[better_than]._forwarded = false;
  Timer *t = new Timer(static_forward_query_hook, (void *) this);
  t->initialize(this);
  
  t->schedule_at(_seen[si]._queries[better_than]._to_send);
}

void
Top5::forward_query_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _seen.size(); x++) {
    for (int y = 0; y < _seen[x]._queries.size(); y++) {
      if (timercmp(&_seen[x]._queries[y]._to_send, &now, <) 
	  && !_seen[x]._queries[y]._forwarded) {
	forward_query(&_seen[x]._queries[y]);
      }
    }
  }
}
void
Top5::forward_query(Query *q)
{
  click_chatter("Top5 %s: forward_query %s -> %s\n", 
		id().cc(),
		q->_src.s().cc(),
		q->_dst.s().cc());
  int nhops = q->_p.size();

  top5_assert(q->_p.size() == q->_metrics.size()+1);

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
  pk->_qdst = q->_dst;
  pk->_seq = htonl(q->_seq);
  pk->set_num_hops(nhops);

  for(int i=0; i < nhops; i++) {
    pk->set_hop(i, q->_p[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_metric(i, q->_metrics[i]);
  }

  q->_forwarded = true;
  send(p);
}

// Continue unicasting a reply packet.
void
Top5::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  top5_assert(type == PT_REPLY);

  _link_table->dijkstra();
  click_chatter("Top5 %s: forward_reply %s <- %s\n", 
		id().cc(),
		pk1->get_hop(0).s().cc(),
		IPAddress(pk1->_qdst).s().cc());
  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("Top5 %s: forward_reply strange next=%d, nhops=%d", 
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
void
Top5::start_reply_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _replies.size(); x++) {
    if (timercmp(&_replies[x]._to_send, &now, <) 
	&& !_replies[x]._sent) {
      start_reply(&_replies[x]);
    }
  }
}

void Top5::start_reply(class Reply *r) 
{

  _link_table->dijkstra();
  Vector<Path> paths = _link_table->top_n_routes(r->_dst, TOP_N);
  
  int num_hosts = 0;
  for (int x = 0; x < paths.size(); x++) {
    num_hosts += paths[x].size();
  }
  int len = sr_pkt::len_wo_data(num_hosts);
  click_chatter("Top5 %s: start_reply %s <- %s\n",
		id().cc(),
		r->_src.s().cc(),
		r->_dst.s().cc());
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;

  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk_out = (struct sr_pkt *) (eh+1);
  memset(pk_out, '\0', len);


  pk_out->_version = _srcr_version;
  pk_out->_type = PT_REPLY;
  pk_out->_flags = 0;
  pk_out->_seq = r->_seq;
  pk_out->set_num_hops(num_hosts);
  pk_out->set_next(paths[0].size() - 1);
  pk_out->_qdst = _ip;

  int hop = 0;
  for (int x = 0; x < paths.size(); x++) {
    for (int y = 0; y < paths[x].size(); y++) {
      IPAddress ip = paths[x][y];
      pk_out->set_hop(hop++, ip);
    }
  }

  for (int x = 0; x < hop; x++) {
    int metric = _link_table->get_hop_metric(pk_out->get_hop(x), 
					    pk_out->get_hop(x+1));
    pk_out->set_metric(x, metric);
  }
  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
Top5::got_reply(struct sr_pkt *pk)
{

  IPAddress dst = IPAddress(pk->_qdst);

  click_chatter("Top5 %s: got_reply %s <- %s\n", 
		id().cc(),
		_ip.s().cc(),
		dst.s().cc());
  Vector<Path> paths;
  paths = _link_table->top_n_routes(dst, TOP_N);

  Dst *d = _dsts.findp(dst);
  if (!d) {
    _dsts.insert(dst, Dst());
    d = _dsts.findp(dst);
    d->_ip = dst;
  }
  d->_paths = paths;
  for (int x = 0; x < paths.size(); x++) {
    d->_count.push_back(0);
  }
  d->_current_path = 0;

}


void
Top5::process_data(Packet *)
{
  return;

}
void
Top5::push(int port, Packet *p_in)
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
    IPAddress dst = p_in->dst_ip_anno();
    top5_assert(dst);
    Dst *d = _dsts.findp(dst);

    if (!d) {
      click_chatter("Top5 %s: no dst found for %s\n",
		    id().cc(),
		    dst.s().cc());
      p_in->kill();
      return;
    }
    
    Path p = d->_paths[d->_current_path];
    Packet *p_out = _srcr->encap(p_in->data(), p_in->length(), p);
    output(1).push(p_out);
    p_in->kill();
   
  } else if (port ==  0) {
    click_ether *eh = (click_ether *) p_in->data();
    struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
    if(eh->ether_type != htons(_et)) {
      click_chatter("Top5 %s: bad ether_type %04x",
		    _ip.s().cc(),
		    ntohs(eh->ether_type));
      p_in->kill();
      return;
    }
    if (EtherAddress(eh->ether_shost) == _en) {
      click_chatter("Top5 %s: packet from me");
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
      top5_assert(0);
    }

    _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
    update_link(_ip, neighbor, get_metric(neighbor));
    if(type == PT_QUERY){
      process_query(pk);
      
    } else if(type == PT_REPLY){
      if(pk->get_hop(pk->next()) != _ip){
	// it's not for me. these are supposed to be unicast,
	// so how did this get to me?
	click_chatter("Top5 %s: reply not for me %d/%d %s",
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
      click_chatter("Top5 %s: bad sr_pkt type=%x",
		    _ip.s().cc(),
		    type);
    }
    
  }
  p_in->kill();
  return;
  


}


String
Top5::static_print_stats(Element *f, void *)
{
  Top5 *d = (Top5 *) f;
  return d->print_stats();
}

String
Top5::print_stats()
{
  
  return
    String(_num_queries) + " queries sent\n" +
    String(_bytes_queries) + " bytes of query sent\n" +
    String(_num_replies) + " replies sent\n" +
    String(_bytes_replies) + " bytes of reply sent\n";
}

int
Top5::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  Top5 *n = (Top5 *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
Top5::clear() 
{
  _link_table->clear();
  _seen.clear();

  _num_queries = 0;
  _bytes_queries = 0;
  _num_replies = 0;
  _bytes_replies = 0;
}

int
Top5::static_start(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  Top5 *n = (Top5 *) e;
  IPAddress dst;

  if (!cp_ip_address(arg, &dst))
    return errh->error("dst must be an IPAddress");

  n->start(dst);
  return 0;
}
void
Top5::start(IPAddress dst) 
{
  click_chatter("write handler called with dst %s", dst.s().cc());
  start_query(dst);
}


void
Top5::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("start", static_start, 0);
}

void
Top5::top5_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("Top5 %s assertion \"%s\" failed: file %s, line %d",
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
template class Vector<Top5::IPAddress>;
template class DEQueue<Top5::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(Top5)

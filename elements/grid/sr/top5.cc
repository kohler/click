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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "srforwarder.hh"
#include "srcrstat.hh"
#include "top5.hh"


CLICK_DECLS

#ifndef top5_assert
#define top5_assert(e) ((e) ? (void) 0 : top5_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


Top5::Top5()
  :  Element(3,2),
     _et(0),
     _ip(),
     _en(),
     _timer(this), 
     _sr_forwarder(0),
     _link_table(0),
     _srcr_stat(0),
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
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    "SS", cpElement, "SrcrStat element", &_srcr_stat,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (!_sr_forwarder) 
    return errh->error("SR not specified");
  if (!_link_table) 
    return errh->error("LinkTable LT not specified");
  if (!_arp_table) 
    return errh->error("ARPTable not specified");


  if (_sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SR element is not a SRForwarder");
  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  if (_srcr_stat && _srcr_stat->cast("SrcrStat") == 0) 
    return errh->error("SS element is not a SrcrStat");

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
  _timer.schedule_after_ms(1000);
}

void
Top5::start_query(IPAddress dstip)
{
  click_chatter("Top5 %s: start_query %s ->  %s", 
		id().cc(),
		_ip.s().cc(),
		dstip.s().cc());


  Dst *d = _dsts.findp(dstip);
  if (!d) {
    _dsts.insert(dstip, Dst());
    d = _dsts.findp(dstip);
    d->_ip = dstip;
    d->_started = false;
  }
  int len = srpacket::len_wo_data(1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr_version;
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
  struct srpacket *pk = (struct srpacket *) (eh+1);

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
  int metric = 9999;
  
  if (_srcr_stat) {
    metric = _srcr_stat->get_etx(other);
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
Top5::process_query(struct srpacket *pk1)
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
      _replies[x]._src = src;
      _replies[x]._dst = dst;
      _replies[x]._seq = seq;
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
  if (_seen[si]._metric && _seen[si]._metric <= metric) {
    /* the metric is worse that what we've seen*/
    return;
  }

  _seen[si]._metric = metric;
  click_gettimeofday(&_seen[si]._when);

  _seen[si]._hops = hops;
  _seen[si]._metrics = metrics;
  struct extra_link_info *extra = (struct extra_link_info *) pk1->data();

  for (int x = 0; x < extra->num_hops(); x++) {
    _seen[si]._extra_hosts.push_back(extra->get_hop(x));
    if (x != extra->num_hops()-1) {
      _seen[si]._extra_metrics.push_back(extra->get_metric(x));
    }
  }

  if (dst == _ip) {
    /* query for me */
    //start_reply(pk1);
  } else {
    _seen[si]._hops = hops;
    _seen[si]._metrics = metrics;
    if (timercmp(&_seen[si]._when, &_seen[si]._to_send, <)) {
      /* a timer has already been scheduled */
      return;
    } else {
      /* schedule timer */
      int delay_time = random() % 750 + 1;
      top5_assert(delay_time > 0);

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
Top5::forward_query_hook() 
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
Top5::forward_query(Seen *s)
{
  click_chatter("Top5 %s: forward_query %s -> %s\n", 
		id().cc(),
		s->_src.s().cc(),
		s->_dst.s().cc());
  int nhops = s->_hops.size();
  int extra_num_hops = s->_extra_hosts.size()+5;
  top5_assert(s->_hops.size() == s->_metrics.size()+1);
  
  //click_chatter("forward query called");
  int len = srpacket::len_wo_data(nhops) + 
    extra_link_info::len(extra_num_hops);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr_version;
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

  struct extra_link_info *extra = (struct extra_link_info *) pk->data();
  extra->set_num_hops(extra_num_hops);
  for (int i = 0; i < extra_num_hops; i++) {
    extra->set_hop(i, s->_extra_metrics[i]);
    if (i != extra_num_hops - 1) {
      extra->set_metric(i, s->_extra_metrics[i]);
    } 
  }
  
  for (int i = 0; i < 5; i += 2) {
    /* get random link, add to extra */
    LinkTable::Link l = _link_table->random_link();
    extra->set_hop(i, l._from);
    extra->set_metric(i, l._metric);
    extra->set_hop(i+1, l._to);
    if (i > 0) {
      /* set the intermediate ho if we have that information */
      IPAddress f = extra->get_hop(i-1);
      int extra_metric = _link_table->get_hop_metric(l._from, f);
      extra->set_metric(i-1, extra_metric);
    }
  }
  s->_forwarded = true;
  send(p);
}

// Continue unicasting a reply packet.
void
Top5::forward_reply(struct srpacket *pk1)
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
  struct srpacket *pk = (struct srpacket *) (eh+1);
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
  
  int num_hosts = paths[0].size();
  if (num_hosts < 2) {
      click_chatter("Top5 %s: not path for reply %s <- %s\n",
		id().cc(),
		r->_src.s().cc(),
		r->_dst.s().cc());
      return;
  }

  int extra_num_hosts = 0;

  for (int x = 1; x < paths.size(); x++ ) {
    extra_num_hosts += paths[x].size();
  }

  int len = srpacket::len_wo_data(num_hosts) +
    extra_link_info::len(extra_num_hosts);

  click_chatter("Top5 %s: start_reply %s <- %s\n",
		id().cc(),
		r->_src.s().cc(),
		r->_dst.s().cc());
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;

  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk_out = (struct srpacket *) (eh+1);
  memset(pk_out, '\0', len);


  pk_out->_version = _sr_version;
  pk_out->_type = PT_REPLY;
  pk_out->_flags = 0;
  pk_out->_seq = r->_seq;
  pk_out->set_num_hops(num_hosts);
  pk_out->set_next(paths[0].size() - 1);
  pk_out->_qdst = _ip;

  for (int x = 0; x < paths[0].size(); x++) {
      IPAddress ip = paths[0][x];
      pk_out->set_hop(x, ip);
  }

  for (int x = 0; x < paths[0].size()-1; x++) {
    int metric = _link_table->get_hop_metric(pk_out->get_hop(x), 
					     pk_out->get_hop(x+1));
    pk_out->set_metric(x, metric);
  }


  struct extra_link_info *extra = (struct extra_link_info *) pk_out->data();
  extra->set_num_hops(extra_num_hosts);
  int current_hop = 0;

  for (int x = 0; x < paths.size(); x++) {
    for (int y = 0; y < paths[x].size(); y += 2) {
      IPAddress from = paths[x][y];
      IPAddress to = paths[x][y+1];
      int metric = _link_table->get_hop_metric(from, to);
      extra->set_hop(current_hop, from);
      extra->set_hop(current_hop+1, to);
      extra->set_metric(current_hop, metric);
      current_hop += 2;
    }
  }
  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
Top5::got_reply(struct srpacket *pk)
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
      start(dst);
      p_in->kill();
      return;
    }

    if (!d->_started) {
      click_chatter("Top5 %s: dst %s hasn't started yet\n",
		    id().cc(),
		    dst.s().cc());

      p_in->kill();
      return;
    }
    
    Path p = d->_paths[d->_current_path];
    Packet *p_out = _sr_forwarder->encap(p_in->data(), p_in->length(), p);
    output(1).push(p_out);
    p_in->kill();
   
  } else if (port ==  0) {
    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
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
      click_chatter("Top5 %s: bad srpacket type=%x",
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

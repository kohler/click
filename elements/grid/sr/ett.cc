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
  :  Element(4,2),
     _timer(this), 
     _gw("255.255.255.255"),
     _rx_stats(0),
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
  _reply_wait.tv_sec = 2;
  _reply_wait.tv_usec = 0;

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
  _is_gw = false;
  ret = cp_va_parse(conf, this, errh,
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIPAddress, "IP address", &_ip,
		    cpEtherAddress, "EtherAddress", &_en,
		    cpElement, "SRCR element", &_srcr,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
		    "GW", cpBool, "Gateway", &_is_gw,
		    "RX", cpElement, "RXStats element", &_rx_stats,
                    0);

  if (_srcr && _srcr->cast("SRCR") == 0) 
    return errh->error("SRCR element is not a SRCR");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_rx_stats && _rx_stats->cast("RXStats") == 0) 
    return errh->error("RX element is not a RXStats");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a Arptable");

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
  _timer.schedule_after_ms(1000);
}

void
ETT::start_query(IPAddress dstip)
{
  Query *q = _queries.findp(dstip);
  if (!q) {
    Query foo = Query(dstip);
    _queries.insert(dstip, foo);
    q = _queries.findp(dstip);
  }

  q->_seq = _seq;
  click_gettimeofday(&q->_last_query);

  click_chatter("ETT %s : starting query for %s", 
		_ip.s().cc(),
		dstip.s().cc());

  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
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
  struct sr_pkt *pk = (struct sr_pkt *) p->data();

  pk->ether_type = htons(_et);
  memcpy(pk->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    memset(pk->ether_dhost, 0xff, 6);
    _num_queries++;
    _bytes_queries += p->length();
  } else if(type == PT_REPLY){
    u_short next = ntohs(pk->_next);
    ett_assert(next < MaxHops + 2);
    EtherAddress eth_dest = _arp_table->lookup(pk->get_hop(next));
    memcpy(pk->ether_dhost, eth_dest.data(), 6);
    _num_replies++;
    _bytes_replies += p->length();
  } else {
    ett_assert(0);
    return;
  }

  output(0).push(p);
}


int
ETT::get_metric(IPAddress neighbor)
{
  EtherAddress eth;
  eth = _arp_table->lookup(neighbor);
  if (_rx_stats && eth != _bcast) {
    return rate_to_metric(_rx_stats->get_rate(eth));
  }
  return rate_to_metric(0);
}

void
ETT::update_link(IPAddress from, IPAddress to, int metric) {
  _link_table->update_link(from, to, metric);
}
static inline bool power_of_two(int x)
{
  int p = 1;

  while (x < p) {
    p *= 2;
  }
  return (x == p);
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
  Vector<u_short> metrics;
  int metric = 0;
  for(int i = 0; i < pk1->num_hops(); i++) {
    IPAddress hop = IPAddress(pk1->get_hop(i));
    if (i != pk1->num_hops()-1) {
      metrics.push_back(pk1->get_fwd_metric(i));
      metric += pk1->get_fwd_metric(i);
    }
    hops.push_back(hop);
    if (hop == _ip) {
      click_chatter("ETT %s: I'm already in this query from %s to %s!  - dropping\n",
		    _ip.s().cc(),
		    src.s().cc(), dst.s().cc());
      /* I'm already in this route! */
      return;
    }
  }
  /* also get the metric from the neighbor */
  metric += get_metric(pk1->get_hop(pk1->num_hops()-1));

  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq){
      break;
    }
  }

  if (si == _seen.size()) {
    _seen.push_back(Seen(src, dst, seq, 0));
  }
  _seen[si]._count++;
  if (_seen[si]._metric >= metric) {
    /* the metric is worse that what we've seen*/
    click_chatter("ETT %s: dropping poor metric %d from %s\n",
		  _ip.s().cc(),
		  metric,
		  src.s().cc());
    return;
  }

  click_chatter("ETT %s: better query for me from %s with seq %d\n", 
		_ip.s().cc(),
		src.s().cc(), seq);
  click_gettimeofday(&_seen[si]._when);
  
  if (dst == _ip || (dst == IPAddress("255.255.255.255") && _is_gw)) {
    /* query for me */
    click_chatter("ETT %s: got a query for me from %s\n", 
		  _ip.s().cc(),
		    src.s().cc());
    
    start_reply(src, dst, seq);
  } else {
    /* query for someone else */
    click_chatter("ETT %s: forwarding immediately", _ip.s().cc());
    forward_query(_seen[si], hops, metrics);
  }

}
void
ETT::forward_query(Seen s, Vector<IPAddress> hops, Vector<u_short> metrics)
{
  u_short nhops = hops.size();

  //click_chatter("forward query called");
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
  pk->set_num_hops(nhops);

  for(int i=0; i < nhops; i++) {
    pk->set_hop(i, hops[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_fwd_metric(i, metrics[i]);
  }


  send(p);
}

// Continue unicasting a reply packet.
void
ETT::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  
  ett_assert(type == PT_REPLY);
  click_chatter("ETT %s: forwarding reply\n", _ip.s().cc());
  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("ETT %s: forward_reply strange next=%d, nhops=%d", 
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

void ETT::start_reply(IPAddress src, IPAddress dst, u_long seq)
{
  click_chatter("ETT %s: start_reply called for %s\n", _ip.s().cc(), 
		src.s().cc());
  _link_table->dijkstra();
  click_chatter("ETT %s: finished running dijkstra\n", _ip.s().cc());
  Path path = _link_table->best_route(src);
  click_chatter("ETT %s: start_reply: found path to %s: [%s]\n", 
		_ip.s().cc(),
		src.s().cc(), path_to_string(path).cc());
  if (!_link_table->valid_route(path)) {
    click_chatter("ETT %s: no valid route to reply for %s: starting query\n",
		  _ip.s().cc(),
		  src.s().cc());
    start_query(src);

    return;
  }


  int len = sr_pkt::len_wo_data(path.size());
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;

  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_REPLY;
  pk->_flags = 0;
  pk->_qdst = dst;
  pk->_seq = seq;
  pk->set_num_hops(path.size());
  int i;
  for(i = 0; i < path.size(); i++) {
    pk->set_hop(i, path[path.size() - 1 - i]);
    //click_chatter("reply: set hop %d to %s\n", i, pk->get_hop(i).s().cc());
  }

  for(i = 0; i < path.size() - 1; i++) {
    u_short m = _link_table->get_hop_metric(pk->get_hop(i), pk->get_hop(i+1));
    pk->set_fwd_metric(i, m);
  }

  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
ETT::got_reply(struct sr_pkt *pk)
{

  IPAddress dst = IPAddress(pk->_qdst);

  click_chatter("ETT: got reply from %s\n", dst.s().cc());
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
  if (!q->_metric || q->_metric > metric) {
    q->_metric = metric;
    if (dst == IPAddress("255.255.255.255")) {
      _gw = pk->get_hop(pk->num_hops() - 1);
    }
  }
}


String 
ETT::route_to_string(Vector<IPAddress> s) 
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
ETT::push(int port, Packet *p_in)
{
  if (port == 2 || port == 3) {
    bool sent_packet = false;
    IPAddress dst = p_in->dst_ip_anno();
    if (port == 3) {
      dst = _gw;
    }
    
    ett_assert(dst);
    Path p = _link_table->best_route(dst);
    int metric = _link_table->get_route_metric(p);

    if (dst != IPAddress("255.255.255.255")) {

      if (_link_table->valid_route(p)) {
	Packet *p_out = _srcr->encap(p_in->data(), p_in->length(), p);
	if (p_out) {
	  sent_packet = true;
	  output(1).push(p_out);
	}
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
    
    if (sent_packet && q->_metric && metric * 2 > q->_metric) {
      return;
    }
    //click_chatter("ETT %s: need route to %s\n", _ip.s().cc(), dst.s().cc());
    
    struct timeval n;
    click_gettimeofday(&n);
    struct timeval expire;
    timeradd(&q->_last_query, &_query_wait, &expire);
    if (timercmp(&expire, &n, <)) {
      click_chatter("ETT %s: calling start_query to %s\n", 
		    _ip.s().cc(), 
		    dst.s().cc());
      start_query(dst);
    
    } else {
	//click_chatter("recent query to %s, so discarding packet\n", dst.s().cc());
    }
    return;
  } else if (port ==  0 || port == 1) {
    struct sr_pkt *pk = (struct sr_pkt *) p_in->data();
    if(p_in->length() < 20 || p_in->length() < pk->hlen_wo_data()){
      click_chatter("ETT %s: bad sr_pkt len %d, expected %d",
		    _ip.s().cc(),
		    p_in->length(),
		    pk->hlen_wo_data());
      p_in->kill();
      return;
    }
    if(pk->ether_type != htons(_et)) {
      click_chatter("ETT %s: bad ether_type %04x",
		    _ip.s().cc(),
		    ntohs(pk->ether_type));
      p_in->kill();
      return;
    }
    if (pk->get_shost() == _en) {
      click_chatter("ETT %s: packet from me");
      p_in->kill();
      return;
    }
    
    u_char type = pk->_type;
    
    /* update the metrics from the packet */
    for(int i = 0; i < pk->num_hops()-1; i++) {
      IPAddress a = pk->get_hop(i);
      IPAddress b = pk->get_hop(i+1);
      u_short m = pk->get_fwd_metric(i);
      click_chatter("ETT %s: on address a= %s\n", 
		    _ip.s().cc(), 
		    a.s().cc());
      if (m != 0) {
	click_chatter("ETT %s: updating metric %i %s <%d> %s", 
		      _ip.s().cc(),
		      i, a.s().cc(), m, b.s().cc());
	update_link(a,b,m);
      }
    }
    
    
    if (port == 1) {
      p_in->kill();
      return;
    }
    IPAddress neighbor = IPAddress();
    switch (type) {
    case PT_QUERY:
      neighbor = pk->get_hop(pk->num_hops()-1);
      click_chatter("ETT %s: q neighbor = %s, index %d\n", 
		    _ip.s().cc(),
		    neighbor.s().cc(), pk->num_hops()-1);
      if (_link_table->get_hop_metric(_ip, neighbor) == 0) {
	click_chatter("ETT %s: updating reverse link to %s", 
		      _ip.s().cc(),
		      neighbor.s().cc());
	update_link(_ip, neighbor, rate_to_metric(0));
      }
      break;
    case PT_REPLY:
      neighbor = pk->get_hop(pk->next()+1);
      click_chatter("ETT %s: r neighbor = %s, index %d\n", 
		    _ip.s().cc(),
		    neighbor.s().cc(), pk->num_hops()+1);
      break;
    default:
      ett_assert(0);
    }
    _arp_table->insert(neighbor, pk->get_shost());
    u_short m = get_metric(neighbor);
    click_chatter("ETT %s: updating neighbor %s <%d> %s", 
		  _ip.s().cc(),
		  neighbor.s().cc(), m, _ip.s().cc());
    update_link(neighbor, _ip, m);
    
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

String
ETT::static_print_is_gateway(Element *f, void *)
{
  ETT *d = (ETT *) f;
  return d->print_is_gateway();
}

String
ETT::print_is_gateway()
{
  
  if (_is_gw) {
    return "true\n";
  }
  return "false\n";
}

String
ETT::static_print_current_gateway(Element *f, void *)
{
  ETT *d = (ETT *) f;
  return d->print_current_gateway();
}

String
ETT::print_current_gateway()
{
  
  return _gw.s() + "\n";
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

void
ETT::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_read_handler("current_gateway", static_print_current_gateway, 0);
  add_read_handler("is_gateway", static_print_is_gateway, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("start", static_start, 0);
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
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<ETT::IPAddress>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(ETT)

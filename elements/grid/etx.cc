/*
 * ETX.{cc,hh} -- DSR implementation
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
#include "etx.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef etx_assert
#define etx_assert(e) ((e) ? (void) 0 : etx_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


ETX::ETX()
  :  Element(2,2),
     _timer(this), 
     _link_stat(0),
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

}

ETX::~ETX()
{
  MOD_DEC_USE_COUNT;
}

int
ETX::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIP6Address, "IP address", &_ip,
                    cpEthernetAddress, "Ethernet address", &_en,
		    cpElement, "SRCR element", &_srcr,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
		    cpBool, "Gateway", &_is_gw,
                    cpKeywords,
                    "LS", cpElement, "LinkStat element", &_link_stat,
                    0);

  if (_srcr && _srcr->cast("SRCR") == 0) 
    return errh->error("SRCR element is not a SRCR");
  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a ARPTable");
  if (_link_stat && _link_stat->cast("LinkStat") == 0) 
    return errh->error("LS element is not a LinkStat");

  return ret;
}

ETX *
ETX::clone () const
{
  return new ETX;
}

int
ETX::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
ETX::run_timer ()
{
  _timer.schedule_after_ms(1000);
}

void
ETX::start_query(IP6Address dstip)
{
  Query *q = _queries.findp(dstip);
  if (!q) {
    Query foo = Query(dstip);
    _queries.insert(dstip, foo);
    q = _queries.findp(dstip);
  }

  q->_seq = _seq;
  click_gettimeofday(&q->_last_query);

  click_chatter("ETX: starting query for %s", dstip.s().cc());

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
  pk->_nhops = htons(1);
  pk->set_hop(0,_ip);
  send(p);
}



// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
ETX::send(WritablePacket *p)
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
    etx_assert(next < MaxHops + 2);
    EtherAddress eth_dest = _arp_table->lookup(pk->get_hop(next));
    memcpy(pk->ether_dhost, eth_dest.data(), 6);
    _num_replies++;
    _bytes_replies += p->length();
  } else {
    etx_assert(0);
    return;
  }

  output(0).push(p);
}

// Ask LinkStat for the metric for the link from other to us.
u_short
ETX::get_metric(IP6Address )
{
  return 0;
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
ETX::process_query(struct sr_pkt *pk1)
{
  IP6Address src(pk1->get_hop(0));
  IP6Address dst(pk1->_qdst);
  u_long seq = ntohl(pk1->_seq);
  int si;

  Vector<IP6Address> hops;
  Vector<u_short> metrics;
  for(int i = 0; i < pk1->num_hops(); i++) {
    IP6Address hop = IP6Address(pk1->get_hop(i));
    if (i != pk1->num_hops()-1) {
      metrics.push_back(pk1->get_fwd_metric(i));
    }
    hops.push_back(hop);
    if (hop == _ip) {
      click_chatter("I'm already in this query from %s to %s!  - dropping\n",
		    src.s().cc(), dst.s().cc());
      /* I'm already in this route! */
      return;
    }
  }

  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq){
      break;
    }
  }

  if (si == _seen.size()) {
    _seen.push_back(Seen(src, dst, seq));
  }
  _seen[si]._count++;

  if (dst == _ip || (dst == IP6Address("255.255.255.255") && _is_gw)) {
    /* query for me */
    click_chatter("got a query for me from %s\n", src.s().cc());
    if (_seen[si]._count > 1) {
      click_chatter("already seen query\n");
      return;
    } 
    click_chatter("1st query for me from %s with seq %d\n", src.s().cc(), seq);
    click_gettimeofday(&_seen[si]._when);

    Timer *t = new Timer(static_reply_hook, (void *) this);
    t->initialize(this);
    struct timeval expire;
    timeradd(&_seen[si]._when, &_reply_wait, &expire);
    t->schedule_at(expire);
    return;
  } 
  /* query for someone else */
  if  (power_of_two(_seen[si]._count)) { 
    click_chatter("ETX %s: forwarding immediately", _ip.s().cc());
    forward_query(_seen[si], hops, metrics);
  } else {
    click_chatter("ETX %s: not forwarding", _ip.s().cc());
  }

}
void
ETX::forward_query(Seen s, Vector<IP6Address> hops, Vector<u_short> metrics)
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
  pk->_nhops = htons(nhops);

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
ETX::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  
  etx_assert(type == PT_REPLY);
  click_chatter("ETX: forwarding reply\n");
  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("ETX %s: forward_reply strange next=%d, nhops=%d", 
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
void 
ETX::reply_hook(Timer *t)
{
  etx_assert(t);
  struct timeval now;

  click_gettimeofday(&now);

  click_chatter("reply_hook called\n");
  for(int x = 0; x < _seen.size(); x++) {
    if (_seen[x]._dst == _ip || (_seen[x]._dst == _bcast_ip && _gw)) {
      click_chatter("on src %s\n", _seen[x]._src.s().cc());
      struct timeval expire;
      timeradd(&_seen[x]._when, &_reply_wait, &expire);
      if (timercmp(&expire, &now, >)) {
	click_chatter("s hasn't expired\n", _seen[x]._src.s().cc());
      } else {
	start_reply(_seen[x]._src, _seen[x]._dst, _seen[x]._seq);
      }
    }
  }

}

void ETX::start_reply(IP6Address src, IP6Address dst, u_long seq)
{
  click_chatter("ETX: start_reply called for %s\n", src.s().cc());
  _link_table->dijkstra();
  Path path = _link_table->best_route(src);
  click_chatter("start_reply: found path to %s: [%s]\n", src.s().cc(), path_to_string(path).cc());
  if (!_link_table->valid_route(path)) {
    click_chatter("couldn't reply to %s becuase no valid route!\n", 
		  src.s().cc());
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
  pk->_nhops = htons(path.size());
  
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
ETX::got_reply(struct sr_pkt *pk)
{
  click_chatter("ETX: got reply from %s\n", IP6Address(pk->_qdst).s().cc());
  _link_table->dijkstra();
}


String 
ETX::route_to_string(Vector<IP6Address> s) 
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
ETX::push(int port, Packet *p_in)
{
  if (port == 2 || port == 3) {
    bool sent_packet = false;
    IP6Address dst = p_in->dst_ip6_anno();
    if (port == 3) {
      dst = _gw;
    }
    Path p = _link_table->best_route(dst);
    if (_link_table->valid_route(p)) {
      Packet *p_out = _srcr->encap(p_in->data(), p_in->length(), p);
      if (p_out) {
	sent_packet = true;
	output(1).push(p_out);
      }
    }
    
    u_short metric = _link_table->get_route_metric(p);
    //click_chatter("no route to %s\n", dst.s().cc());
    Query *q = _queries.findp(dst);
    if (!q) {
      Query foo = Query(dst);
      _queries.insert(dst, foo);
      q = _queries.findp(dst);
    }
    etx_assert(q);
    
    if (sent_packet && q->_metric && metric * 2 > q->_metric) {
      p_in->kill();
      return;
    }
    
    struct timeval n;
    click_gettimeofday(&n);
    struct timeval expire;
    timeradd(&q->_last_query, &_query_wait, &expire);
    if (timercmp(&expire, &n, <)) {
      click_chatter("calling start_query to %s\n", dst.s().cc());
      if (port == 3) {
	start_query(IP6Address("255.255.255.255"));
      } else {
	start_query(dst);
      }
    } else {
	//click_chatter("recent query to %s, so discarding packet\n", dst.s().cc());
    }
    p_in->kill();
    return;
  } else if (port ==  0 || port == 1) {
    struct sr_pkt *pk = (struct sr_pkt *) p_in->data();
    if(p_in->length() < 20 || p_in->length() < pk->hlen_wo_data()){
      click_chatter("ETX %s: bad sr_pkt len %d, expected %d",
		    _ip.s().cc(),
		    p_in->length(),
		    pk->hlen_wo_data());
      p_in->kill();
      return;
    }
    if(pk->ether_type != htons(_et)) {
      click_chatter("ETX %s: bad ether_type %04x",
		    _ip.s().cc(),
		    ntohs(pk->ether_type));
      p_in->kill();
      return;
    }

  u_char type = pk->_type;

  /* update the metrics from the packet */
  unsigned int now = click_jiffies();
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IP6Address a = pk->get_hop(i);
    IP6Address b = pk->get_hop(i+1);
    u_short m = pk->get_fwd_metric(i);
    if (m != 0) {
      click_chatter("updating %s <%d> %s", a.s().cc(), m, b.s().cc());
      _link_table->update_link(a, b, m, now);
    }
  }
  

  if (port == 1) {
    p_in->kill();
    return;
  }
  IP6Address neighbor = IP6Address();
  switch (type) {
  case PT_QUERY:
    neighbor = pk->get_hop(pk->num_hops() - 1);
    break;
  case PT_REPLY:
    neighbor = pk->get_hop(pk->next()+1);
    break;
  default:
    etx_assert(0);
  }
  u_short m = get_metric(neighbor);
  click_chatter("updating %s <%d> %s", neighbor.s().cc(), m, _ip.s().cc());
  _link_table->update_link(neighbor, _ip, m, now);
  _arp_table->insert(neighbor, EtherAddress(pk->ether_shost));
  if(type == PT_QUERY){
      process_query(pk);

  } else if(type == PT_REPLY){
    if(pk->get_hop(pk->next()) != _ip){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("ETX %s: reply not for me %d/%d %s",
                    _ip.s().cc(),
                    pk->next(),
                    pk->num_hops(),
                    pk->get_hop(pk->next()).s().cc());
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
    click_chatter("ETX %s: bad sr_pkt type=%x",
                  _ip.s().cc(),
                  type);
  }

  return;


  }
  p_in->kill();
}


String
ETX::static_print_stats(Element *f, void *)
{
  ETX *d = (ETX *) f;
  return d->print_stats();
}

String
ETX::print_stats()
{
  
  return
    String(_num_queries) + " queries sent\n" +
    String(_bytes_queries) + " bytes of query sent\n" +
    String(_num_replies) + " replies sent\n" +
    String(_bytes_replies) + " bytes of reply sent\n";
}

int
ETX::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ETX *n = (ETX *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
ETX::clear() 
{
  _link_table->clear();
  _seen.clear();

  _num_queries = 0;
  _bytes_queries = 0;
  _num_replies = 0;
  _bytes_replies = 0;
}

int
ETX::static_start(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ETX *n = (ETX *) e;
  IP6Address dst;

  if (!cp_ip6_address(arg, &dst))
    return errh->error("dst must be an IP6Address");

  n->start(dst);
  return 0;
}
void
ETX::start(IP6Address dst) 
{
  click_chatter("write handler called with dst %s", dst.s().cc());
  start_query(dst);
}

void
ETX::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("start", static_start, 0);
}

void
ETX::etx_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("ETX %s assertion \"%s\" failed: file %s, line %d",
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
template class Vector<ETX::IP6Address>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(ETX)

/*
 * ESRCR.{cc,hh} -- DSR implementation
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
#include "esrcr.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef esrcr_assert
#define esrcr_assert(e) ((e) ? (void) 0 : esrcr_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


ESRCR::ESRCR()
  :  _timer(this), 
     _link_stat(0),
     _queries(0), _querybytes(0),
     _replies(0), _replybytes(0)
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

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

ESRCR::~ESRCR()
{
  MOD_DEC_USE_COUNT;
}

int
ESRCR::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIPAddress, "IP address", &_ip,
                    cpEthernetAddress, "Ethernet address", &_en,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
                    "LS", cpElement, "LinkStat element", &_link_stat,
                    "LSNET", cpIPAddress, "LinkStat net", &_ls_net,
                    0);
  return ret;
}

ESRCR *
ESRCR::clone () const
{
  return new ESRCR;
}

int
ESRCR::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
ESRCR::run_timer ()
{
  _timer.schedule_after_ms(1000);
}

void
ESRCR::start_query(IPAddress dstip)
{
  Dst *dst = _dsts.findp(dstip);
  if (!dst) {
    _dsts.insert(dstip, Dst(dstip));
    dst = _dsts.findp(dstip);
  }
  esrcr_assert(dst);


  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;
  struct sr_pkt *pk = (struct sr_pkt *) p->data();
  memset(pk, '\0', len);
  pk->_type = PT_QUERY;
  pk->_flags = 0;
  pk->_qdst = dst->_ip;
  pk->_seq = htonl(++_seq);
  pk->_nhops = htons(1);
  pk->set_hop(0,_ip.in_addr());
  
  dst->_seq = _seq;

  _seen.push_back(Seen(_ip.in_addr(), dstip, pk->_seq));

  send(p);
}



// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
ESRCR::send(WritablePacket *p)
{
  struct sr_pkt *pk = (struct sr_pkt *) p->data();

  pk->ether_type = htons(_et);
  memcpy(pk->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    //memcpy(pk->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
    memcpy(pk->ether_dhost, _bcast.data(), 6);
    _queries++;
    _querybytes += p->length();
  } else if(type == PT_REPLY){
    u_short next = ntohs(pk->_next);
    esrcr_assert(next < MaxHops + 2);
    struct in_addr nxt = pk->get_hop(next);
    EtherAddress eth_dest = _arp_table->lookup(IPAddress(nxt));
    memcpy(pk->ether_dhost, eth_dest.data(), 6);
    _replies++;
    _replybytes += p->length();
  } else {
    esrcr_assert(0);
    return;
  }

  output(1).push(p);
}

// Ask LinkStat for the metric for the link from other to us.
u_short
ESRCR::get_metric(IPAddress next_hop)
{
  IPAddress other = IPAddress(_ls_net.addr() | 
			      (next_hop.addr() & 0xffffff00));
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
ESRCR::process_query(struct sr_pkt *pk1)
{
  IPAddress src(pk1->get_hop(0));
  IPAddress dst(pk1->_qdst);
  u_long seq = ntohl(pk1->_seq);
  int si;

  Vector<IPAddress> hops;
  Vector<u_short> metrics;
  for(int i = 0; i < pk1->num_hops(); i++) {
    IPAddress hop = IPAddress(pk1->get_hop(i));
    if (i != pk1->num_hops()-1) {
      metrics.push_back(pk1->get_metric(i));
    }
    hops.push_back(hop);
    if (hop == _ip) {
      /* I'm already in this route! */
      return;
    }
  }

  if (dst == _ip) {
    Src *s = _srcs.findp(src);
    if (s && s->_seq == seq) {
      return;
    }
    _srcs.insert(dst, Src(dst, seq));
    s = _srcs.findp(src);
    click_gettimeofday(&s->_when);
    Timer *t = new Timer(static_reply_hook, (void *) this);
    t->initialize(this);
    struct timeval expire;
    timeradd(&s->_when, &_reply_wait, &expire);
    t->schedule_at(expire);

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

  if (power_of_two(_seen[si]._count)) { 
    //click_chatter("ESRCR %s: forwarding immediately", _ip.s().cc());
    forward_query(_seen[si], hops, metrics);
  }

}
void
ESRCR::forward_query(Seen s, Vector<IPAddress> hops, Vector<u_short> metrics)
{
  u_short nhops = hops.size();

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
    pk->set_hop(i, hops[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_metric(i, metrics[i]);
  }


  send(p);
}

// Continue unicasting a reply packet.
void
ESRCR::forward_reply(struct sr_pkt *pk1)
{
  u_char type = pk1->_type;
  
  esrcr_assert(type == PT_REPLY);

  if(pk1->next() >= pk1->num_hops()) {
    click_chatter("ESRCR %s: forward_reply strange next=%d, nhops=%d", 
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
ESRCR::reply_hook(Timer *t)
{
  esrcr_assert(t);
  struct timeval now;

  click_gettimeofday(&now);

  SrcTable _src;

  for(SrcTable::iterator iter = _src.begin(); iter; iter++) {
    Src s = iter.value();
    struct timeval expire;
    timeradd(&s._when, &_reply_wait, &expire);
    if (timercmp(&expire, &now, >)) {
      _src.insert(s._ip, s);
    } else {
      start_reply(s._ip);

    }
  }

}
void
ESRCR::start_reply(IPAddress)
{

}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
ESRCR::got_reply(struct sr_pkt *pk)
{
  Dst *dst = _dsts.findp(IPAddress(pk->_qdst));
  if(!dst){
    click_chatter("ESRCR %s: reply but no Dst %s",
                  _ip.s().cc(),
                  IPAddress(pk->_qdst).s().cc());
    return;
  }
  if(ntohl(pk->_seq) != dst->_seq){
    click_chatter("ESRCR %s: reply but wrong seq %d %d",
                  _ip.s().cc(),
                  ntohl(pk->_seq),
                  dst->_seq);
    return;
  }

}


String 
ESRCR::route_to_string(Vector<IPAddress> s) 
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
ESRCR::push(int port, Packet *p_in)
{
  if (port == 1) {
    p_in->kill();
    return;
  } else if (port == 0) {
      struct sr_pkt *pk = (struct sr_pkt *) p_in->data();
  //click_chatter("ESRCR %s: got sr packet", _ip.s().cc());
  if(p_in->length() < 20 || p_in->length() < pk->hlen_wo_data()){
    click_chatter("ESRCR %s: bad sr_pkt len %d, expected %d",
                  _ip.s().cc(),
                  p_in->length(),
		  pk->hlen_wo_data());
    return;
  }
  if(pk->ether_type != htons(_et)) {
    click_chatter("ESRCR %s: bad ether_type %04x",
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
      _link_table->update_link(IPPair(a,b), m, now);
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
  default:
    esrcr_assert(0);
  }
  u_short m = get_metric(neighbor);
  //click_chatter("updating %s <%d> %s", neighbor.s().cc(), m,  _ip.s().cc());
  _link_table->update_link(IPPair(neighbor, _ip), m, now);
  update_best_metrics();

  _arp_table->insert(neighbor, EtherAddress(pk->ether_shost));

  if(type == PT_QUERY){
      process_query(pk);
  } else if(type == PT_REPLY && next < nhops){
    if(pk->get_hop(next) != _ip.in_addr()){
      // it's not for me. these are supposed to be unicast,
      // so how did this get to me?
      click_chatter("ESRCR %s: reply not for me %d/%d %s",
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
  } else {
    click_chatter("ESRCR %s: bad sr_pkt type=%x",
                  _ip.s().cc(),
                  type);
  }

  return;


  }
  p_in->kill();
}


String
ESRCR::static_print_stats(Element *f, void *)
{
  ESRCR *d = (ESRCR *) f;
  return d->print_stats();
}

String
ESRCR::print_stats()
{
  
  return
    String(_queries) + " queries sent\n" +
    String(_querybytes) + " bytes of query sent\n" +
    String(_replies) + " replies sent\n" +
    String(_replybytes) + " bytes of reply sent\n";
}

int
ESRCR::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ESRCR *n = (ESRCR *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
ESRCR::clear() 
{
  _link_table->clear();
  _dsts.clear();
  _seen.clear();

  _queries = 0;
  _querybytes = 0;
  _replies = 0;
  _replybytes = 0;
}

int
ESRCR::static_start(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ESRCR *n = (ESRCR *) e;
  IPAddress dst;

  if (!cp_ip_address(arg, &dst))
    return errh->error("dst must be an IPAddress");

  n->start(dst);
  return 0;
}
void
ESRCR::start(IPAddress dst) 
{
  click_chatter("write handler called with dst %s", dst.s().cc());
  start_query(dst);
}

void
ESRCR::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("start", static_start, 0);
}

void
ESRCR::update_best_metrics() 
{
  _link_table->dijkstra();
}
 void
ESRCR::esrcr_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("ESRCR %s assertion \"%s\" failed: file %s, line %d",
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
template class Vector<ESRCR::IPAddresss>;
template class HashMap<IPAddress, ESRCR::Dst>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(ESRCR)

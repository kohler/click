/*
 * SRQueryForwarder.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrqueryforwarders Institute of Technology
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
#include "srqueryforwarder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "linkmetric.hh"
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "srforwarder.hh"
CLICK_DECLS



SRQueryForwarder::SRQueryForwarder()
  :  Element(1,1),
     _ip(),
     _en(),
     _et(0),
     _sr_forwarder(0),
     _link_table(0),
     _metric(0),
     _arp_table(0)
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

SRQueryForwarder::~SRQueryForwarder()
{
  MOD_DEC_USE_COUNT;
}

int
SRQueryForwarder::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    /* below not required */
		    "LM", cpElement, "LinkMetric element", &_metric,
		    "DEBUG", cpBool, "Debug", &_debug,
                    cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (!_metric) 
    return errh->error("LinkMetric not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (!_arp_table) 
    return errh->error("ARPTable not specified");


  if (_sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SRQueryForwarder element is not a SRQueryForwarder");
  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a ARPTable");
  if (_metric && _metric->cast("LinkMetric") == 0) 
    return errh->error("LinkMetric element is not a LinkMetric");

  return ret;
}

int
SRQueryForwarder::initialize (ErrorHandler *)
{
  return 0;
}

IPAddress
SRQueryForwarder::get_random_neighbor()
{
  if (!_neighbors_v.size()) {
    return IPAddress();
  }
  int ndx = random() % _neighbors_v.size();
  return _neighbors_v[ndx];

}

int
SRQueryForwarder::get_fwd_metric(IPAddress other)
{
  sr_assert(other);
  int metric = 9999;
  if (_metric) {
    metric = _metric->get_fwd_metric(other);
  }
  if (metric && !update_link(_ip, other, 0, metric)) {
    click_chatter("%{element} couldn't update get_fwd_metric %s > %d > %s\n",
		  this,
		  _ip.s().cc(),
		  metric,
		  other.s().cc());
  }
  return metric;
}

int
SRQueryForwarder::get_rev_metric(IPAddress other)
{
  sr_assert(other);
  int metric = 9999;
  if (_metric) {
    metric = _metric->get_rev_metric(other);
  }
  if (metric && !update_link(other, _ip, 0, metric)) {
    click_chatter("%{element} couldn't update get_rev_metric %s > %d > %s\n",
		  this,
		  other.s().cc(),
		  metric,
		  _ip.s().cc());
  }
  return metric;
}

bool
SRQueryForwarder::update_link(IPAddress from, IPAddress to, uint32_t seq, 
			      uint32_t metric) {
  assert(from);
  assert(to);
  assert(metric);
  if (_link_table && !_link_table->update_link(from, to, seq, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().cc(),
		  metric,
		  to.s().cc());
    return false;
  }
  return true;
}

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
SRQueryForwarder::process_query(struct srpacket *pk1)
{
  IPAddress src(pk1->get_link_node(0));
  IPAddress dst(pk1->_qdst);
  u_long seq = ntohl(pk1->_seq);
  int si;

  Vector<IPAddress> hops;
  Vector<int> fwd_metrics;
  Vector<int> rev_metrics;
  Vector<int> seqs;
  if (dst == _ip) {
    /* don't forward queries for me */
    return;
  }
  int fwd_metric = 0;
  int rev_metric = 0;
  for(int i = 0; i < pk1->num_links(); i++) {
    IPAddress hop = IPAddress(pk1->get_link_node(i));
    seqs.push_back(pk1->get_link_seq(i));
    fwd_metrics.push_back(pk1->get_link_fwd(i));
    rev_metrics.push_back(pk1->get_link_rev(i));
    fwd_metric += pk1->get_link_fwd(i);
    rev_metric += pk1->get_link_rev(i);
    hops.push_back(hop);
    if (hop == _ip) {
      /* I'm already in this route! */
      return;
    }
  }
  hops.push_back(pk1->get_link_node(pk1->num_links()));

  _link_table->dijkstra();

  /* also get the metric from the neighbor */
  IPAddress neighbor = pk1->get_link_node(pk1->num_links());
  int fwd_m = get_fwd_metric(neighbor);
  int rev_m = get_rev_metric(neighbor);
  rev_metric += rev_m;
  fwd_metric += fwd_m;

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
    _seen.push_back(Seen(src, dst, seq, 0, 0));
  }
  _seen[si]._count++;
  if (_seen[si]._rev_metric && _seen[si]._rev_metric <= rev_metric) {
    /* the metric is worse that what we've seen*/
    return;
  }

  click_gettimeofday(&_seen[si]._when);
  

  _seen[si]._hops = hops;
  _seen[si]._fwd_metrics = fwd_metrics;
  _seen[si]._rev_metrics = rev_metrics;
  _seen[si]._seqs = seqs;
  if (timercmp(&_seen[si]._when, &_seen[si]._to_send, <)) {
    /* a timer has already been scheduled */
    return;
  } else {
    /* schedule timer */
    int delay_time = random() % 750 + 1;
    sr_assert(delay_time > 0);
    
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
void
SRQueryForwarder::forward_query_hook() 
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
SRQueryForwarder::forward_query(Seen *s)
{
  if (_debug) {
    click_chatter("%{element}: forward_query %s -> %s\n", 
		  this,
		  s->_src.s().cc(),
		  s->_dst.s().cc());
  }
  int links = s->_hops.size();

  sr_assert(s->_hops.size() == s->_fwd_metrics.size()+1);
  sr_assert(s->_hops.size() == s->_rev_metrics.size()+1);
  sr_assert(s->_hops.size() == s->_seqs.size()+1);

  //click_chatter("forward query called");
  int len = srpacket::len_wo_data(links);
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
  pk->set_num_links(links);

  for (int i = 0; i < links - 1; i++) {
    pk->set_link(i,
		 s->_hops[i], s->_hops[i+1],
		 s->_fwd_metrics[i], s->_rev_metrics[i],
		 0,0);
  }
  IPAddress neighbor = s->_hops[links-1];
  click_chatter("neighbor is %s links %d hops %d\n", 
		neighbor.s().cc(),
		links,
		s->_hops.size());
  /* set my neighbor metrics + seq with up to date info */
  pk->set_link(links - 1,
	       neighbor, _ip,
	       (_metric) ? _metric->get_fwd_metric(neighbor) : 0,
	       (_metric) ? _metric->get_rev_metric(neighbor) : 0,
	       (_metric) ? _metric->get_seq(neighbor) : 0,
	       0);

	       
	       
  s->_forwarded = true;
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);
}

void
SRQueryForwarder::push(int, Packet *p_in)
{

  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  if(eh->ether_type != htons(_et)) {
    click_chatter("%{element}: bad ether_type %04x",
		  this,
		  ntohs(eh->ether_type));
    p_in->kill();
    return;
  }
  if (EtherAddress(eh->ether_shost) == _en) {
    click_chatter("%{element}: packet from me",
		  this);
    p_in->kill();
    return;
  }

  u_char type = pk->_type;

  if (type != PT_QUERY) {
    p_in->kill();
    return;
  }

  
  /* update the metrics from the packet */
  for(int i = 0; i < pk->num_links(); i++) {
    IPAddress a = pk->get_link_node(i);
    IPAddress b = pk->get_link_node(i+1);
    int fwd_m = pk->get_link_fwd(i);
    int rev_m = pk->get_link_fwd(i);
    if (a != _ip && b != _ip) {
      /* don't update my immediate neighbor. see below */
      if (fwd_m && !update_link(a,b,0,fwd_m)) {
	click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		      this,
		      a.s().cc(),
		      fwd_m,
		      b.s().cc());
      }
      if (rev_m && !update_link(b,a,0,rev_m)) {
	click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		      this,
		      b.s().cc(),
		      rev_m,
		      a.s().cc());
      }
    }
  }
  
  
  IPAddress neighbor = pk->get_link_node(pk->num_links());
  sr_assert(neighbor);
  
  if (!_neighbors.findp(neighbor)) {
    _neighbors.insert(neighbor, true);
    _neighbors_v.push_back(neighbor);
  }
  
  _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  /* 
   * calling these functions updates the neighbor link 
   * in the link_table, so we can ignore the return value.
   */
  get_fwd_metric(neighbor);
  get_rev_metric(neighbor);
  
  process_query(pk);
  
  p_in->kill();
  return;
  
  

}


enum {H_DEBUG, H_IP, H_CLEAR};

static String 
SRQueryForwarder_read_param(Element *e, void *thunk)
{
  SRQueryForwarder *td = (SRQueryForwarder *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_IP:
    return td->_ip.s() + "\n";
  default:
    return String();
  }
}
static int 
SRQueryForwarder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SRQueryForwarder *f = (SRQueryForwarder *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_CLEAR:
    f->_seen.clear();
    break;
  }
  return 0;
}
void
SRQueryForwarder::add_handlers()
{
  add_read_handler("debug", SRQueryForwarder_read_param, (void *) H_DEBUG);
  add_read_handler("ip", SRQueryForwarder_read_param, (void *) H_IP);

  add_write_handler("debug", SRQueryForwarder_write_param, (void *) H_DEBUG);
  add_write_handler("clear", SRQueryForwarder_write_param, (void *) H_CLEAR);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SRQueryForwarder::IPAddress>;
template class DEQueue<SRQueryForwarder::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRQueryForwarder)

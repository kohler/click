/*
 * GatewaySelector.{cc,hh} -- DSR implementation
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "linkmetric.hh"
#include "gatewayselector.hh"

CLICK_DECLS

GatewaySelector::GatewaySelector()
  :  Element(1,1),
     _ip(),
     _en(),
     _et(0),
     _link_table(0),
     _metric(0),
     _arp_table(0),
     _timer(this)
{
  MOD_INC_USE_COUNT;

  MaxSeen = 200;
  MaxHops = 30;

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

GatewaySelector::~GatewaySelector()
{
  MOD_DEC_USE_COUNT;
}

int
GatewaySelector::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _is_gw = false;
  _period = 15;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    /* not required */
		    "PERIOD", cpUnsigned, "Ad broadcast period (secs)", &_period,
		    "GW", cpBool, "Gateway", &_is_gw,
		    "LM", cpElement, "LinkMetric element", &_metric,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (!_arp_table) 
    return errh->error("ARPTable not specified");


  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  if (_metric && _metric->cast("LinkMetric") == 0) 
    return errh->error("LinkMetric element is not a LinkMetric");

  _gw_expire.tv_usec = 0;
  _gw_expire.tv_sec = _period*4;

  struct timeval now;
  click_gettimeofday(&now);

  return ret;
}

GatewaySelector *
GatewaySelector::clone () const
{
  return new GatewaySelector;
}

int
GatewaySelector::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
GatewaySelector::run_timer ()
{
  if (_is_gw) {
    start_ad();
  }
  struct timeval _next_ad;
  click_gettimeofday(&_next_ad);
  unsigned p = _period * 1000;
  unsigned max_jitter = p / 7;
  long r2 = random();
  unsigned j = (unsigned) ((r2 >> 1) % (max_jitter + 1));
  unsigned int delta_us = 1000 * ((r2 & 1) ? p - j : p + j);
  _next_ad.tv_usec += delta_us;
  _next_ad.tv_sec +=  _next_ad.tv_usec / 1000000;
  _next_ad.tv_usec = (_next_ad.tv_usec % 1000000);
  _timer.schedule_at(_next_ad);
}

void
GatewaySelector::start_ad()
{
  int len = srpacket::len_wo_data(1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr_version;
  pk->_type = PT_GATEWAY;
  pk->_flags = 0;
  pk->_qdst = _ip;
  pk->_seq = htonl(++_seq);
  pk->set_num_hops(1);
  pk->set_hop(0,_ip);
  send(p);
}



// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
GatewaySelector::send(WritablePacket *p)
{
  click_ether *eh = (click_ether *) p->data();
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);
}

int
GatewaySelector::get_fwd_metric(IPAddress other)
{
  int metric = 0;
  sr_assert(other);
  if (_metric) {
    metric = _metric->get_fwd_metric(other);
    if (metric && !update_link(_ip, other, metric)) {
      click_chatter("%{element} couldn't update fwd_metric %s > %d > %s\n",
		    this,
		    _ip.s().cc(),
		    metric,
		    other.s().cc());
    }
    return metric;
  } else {
    return 0;
  }
}


int
GatewaySelector::get_rev_metric(IPAddress other)
{
  int metric = 0;
  sr_assert(other);
  if (_metric) {
    metric = _metric->get_rev_metric(other);
    if (metric && !update_link(other, _ip, metric)) {
      click_chatter("%{element} couldn't update rev_metric %s > %d > %s\n",
		    this,
		    other.s().cc(),
		    metric,
		    _ip.s().cc());
    }
    return metric;
  } else {
    return 0;
  }
}

bool
GatewaySelector::update_link(IPAddress from, IPAddress to, int metric) {
  if (_link_table && !_link_table->update_link(from, to, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().cc(),
		  metric,
		  to.s().cc());
    return false;
  }
  return true;
}

void
GatewaySelector::forward_ad_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _seen.size(); x++) {
    if (timercmp(&_seen[x]._to_send, &now, <) && !_seen[x]._forwarded) {
      forward_ad(&_seen[x]);
    }
  }
}
void
GatewaySelector::forward_ad(Seen *s)
{
  int nhops = s->_hops.size();

  sr_assert(s->_hops.size() == s->_fwd_metrics.size()+1);
  sr_assert(s->_hops.size() == s->_rev_metrics.size()+1);

  int len = srpacket::len_wo_data(nhops);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr_version;
  pk->_type = PT_GATEWAY;
  pk->_flags = 0;
  pk->_qdst = s->_gw;
  pk->_seq = htonl(s->_seq);
  pk->set_num_hops(nhops);

  for(int i=0; i < nhops; i++) {
    pk->set_hop(i, s->_hops[i]);
  }

  for(int i=0; i < nhops - 1; i++) {
    pk->set_fwd_metric(i, s->_fwd_metrics[i]);
    pk->set_rev_metric(i, s->_rev_metrics[i]);
  }

  s->_forwarded = true;
  send(p);
}


IPAddress
GatewaySelector::best_gateway() 
{
  IPAddress best_gw = IPAddress();
  int best_metric = 0;
  struct timeval expire;
  struct timeval now;
  
  _link_table->dijkstra();
  click_gettimeofday(&now);
  
  for(GWIter iter = _gateways.begin(); iter; iter++) {
    GWInfo nfo = iter.value();
    timeradd(&nfo._last_update, &_gw_expire, &expire);
    Path p = _link_table->best_route(nfo._ip);
    int metric = _link_table->get_route_metric(p);
    if (timercmp(&now, &expire, <) && 
	metric && 
	((!best_metric) || best_metric > metric) &&
	!_ignore.findp(nfo._ip) &&
	(!_allow.size() || _allow.findp(nfo._ip))) {
      best_gw = nfo._ip;
      best_metric = metric;
    }
  }
  
  return best_gw;

}
bool
GatewaySelector::valid_gateway(IPAddress gw) 
{
  if (!gw) {
    return false;
  }
  GWInfo *nfo = _gateways.findp(gw);
  struct timeval now;
  struct timeval expire;
  click_gettimeofday(&now);
  
  timeradd(&nfo->_last_update, &_gw_expire, &expire);
  return timercmp(&now, &expire, <);

}

void
GatewaySelector::push(int port, Packet *p_in)
{
  if (port != 0) {
    click_chatter("GatewaySelector %s: bad port %d",
		  id().cc(),
		  port);
    p_in->kill();
    return;
  }
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  if(eh->ether_type != htons(_et)) {
    click_chatter("GatewaySelector %s: bad ether_type %04x",
		  _ip.s().cc(),
		  ntohs(eh->ether_type));
    p_in->kill();
    return;
  }
  if (pk->_type != PT_GATEWAY) {
    click_chatter("GatewaySelector %s: back packet type %d",
		  pk->_type);
    p_in->kill();
    return;
  }
  if (EtherAddress(eh->ether_shost) == _en) {
    click_chatter("GatewaySelector %s: packet from me");
    p_in->kill();
    return;
  }
    
  
  /* update the metrics from the packet */
  for(int i = 0; i < pk->num_hops()-1; i++) {
    IPAddress a = pk->get_hop(i);
    IPAddress b = pk->get_hop(i+1);
    int fwd_m = pk->get_fwd_metric(i);
    int rev_m = pk->get_fwd_metric(i);
    if (a != _ip && b != _ip) {
      /* don't update my immediate neighbor. see below */
      if (fwd_m && !update_link(a,b,fwd_m)) {
	click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		      this,
		      a.s().cc(),
		      fwd_m,
		      b.s().cc());
      }
      if (rev_m && !update_link(b,a,rev_m)) {
	click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		      this,
		      b.s().cc(),
		      rev_m,
		      a.s().cc());
      }
    }

  }
    
    
  IPAddress neighbor = IPAddress();
  neighbor = pk->get_hop(pk->num_hops()-1);

  _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  /* 
   * calling these functions updates the neighbor link 
   * in the link_table, so we can ignore the return value.
   */
  get_fwd_metric(neighbor);
  get_rev_metric(neighbor);


  Vector<IPAddress> hops;
  Vector<int> fwd_metrics;
  Vector<int> rev_metrics;

  int fwd_metric = 0;
  int rev_metric = 0;
  for(int i = 0; i < pk->num_hops(); i++) {
    IPAddress hop = IPAddress(pk->get_hop(i));
    if (i != pk->num_hops() - 1) {
      int fwd_m = pk->get_fwd_metric(i);
      int rev_m = pk->get_rev_metric(i);
      fwd_metrics.push_back(fwd_m);
      rev_metrics.push_back(rev_m);
      fwd_metric += fwd_m;
      rev_metric += rev_m;
    }
    hops.push_back(hop);
    if (hop == _ip) {
      /* I'm already in this route! */
      p_in->kill();
      return;
    }
  }
  /* also get the metric from the neighbor */
  int fwd_m = get_fwd_metric(pk->get_hop(pk->num_hops()-1));
  int rev_m = get_rev_metric(pk->get_hop(pk->num_hops()-1));
  fwd_metric += fwd_m;
  rev_metric += rev_m;
  fwd_metrics.push_back(fwd_m);
  rev_metrics.push_back(rev_m);
  hops.push_back(_ip);

  IPAddress gw = pk->_qdst;
  sr_assert(gw);
  GWInfo *nfo = _gateways.findp(gw);
  if (!nfo) {
    _gateways.insert(gw, GWInfo());
    nfo = _gateways.findp(gw);
  }
  nfo->_ip = gw;
  click_gettimeofday(&nfo->_last_update);
  

  int si = 0;
  u_long seq = ntohl(pk->_seq);
  for(si = 0; si < _seen.size(); si++){
    if(gw == _seen[si]._gw && seq == _seen[si]._seq){
      break;
    }
  }

  if (si == _seen.size()) {
    if (_seen.size() == 100) {
      _seen.pop_front();
      si--;
    }
    _seen.push_back(Seen(gw, seq, 0, 0));
  }
  _seen[si]._count++;
  if (_seen[si]._rev_metric && _seen[si]._rev_metric <= rev_metric) {
    /* the metric is worse that what we've seen*/
    p_in->kill();
    return;
  }

  _seen[si]._fwd_metric = fwd_metric;
  _seen[si]._rev_metric = rev_metric;

  click_gettimeofday(&_seen[si]._when);
  
  _seen[si]._hops = hops;
  _seen[si]._fwd_metrics = fwd_metrics;
  _seen[si]._rev_metrics = rev_metrics;


  if (timercmp(&_seen[si]._when, &_seen[si]._to_send, <)) {
    /* a timer has already been scheduled */
    p_in->kill();
    return;
  } else {
    /* schedule timer */
    int delay_time = (random() % 750) + 1;
    sr_assert(delay_time > 0);
    
    struct timeval delay;
    delay.tv_sec = 0;
    delay.tv_usec = delay_time*1000;
    timeradd(&_seen[si]._when, &delay, &_seen[si]._to_send);
    _seen[si]._forwarded = false;
    Timer *t = new Timer(static_forward_ad_hook, (void *) this);
    t->initialize(this);
    
    t->schedule_at(_seen[si]._to_send);
  }
  

  p_in->kill();
  return;
}


String
GatewaySelector::print_gateway_stats()
{
    StringAccum sa;
    struct timeval now;
    click_gettimeofday(&now);
    for(GWIter iter = _gateways.begin(); iter; iter++) {
      GWInfo nfo = iter.value();
      sa << nfo._ip.s().cc() << " ";
      sa << now - nfo._last_update << " ";
      
      Path p = _link_table->best_route(nfo._ip);
      int metric = _link_table->get_route_metric(p);
      sa << metric << "\n";
    }
    
  return sa.take_string();

}

String
GatewaySelector::read_param(Element *e, void *vparam)
{
  GatewaySelector *f = (GatewaySelector *) e;
  switch ((int)vparam) {
  case 0:			
    return String(f->_is_gw) + "\n";
  case 1:			
    return f->print_gateway_stats();
  case 2: { //ignore
    StringAccum sa;
    for (IPIter iter = f->_ignore.begin(); iter; iter++) {
      IPAddress ip = iter.key();
      sa << ip << "\n";
    }
    return sa.take_string();
  }

  case 3: { //allow
    StringAccum sa;
    for (IPIter iter = f->_allow.begin(); iter; iter++) {
      IPAddress ip = iter.key();
      sa << ip << "\n";
    }
    return sa.take_string();
  }
    
  default:
    return "";
  }
  
}
int
GatewaySelector::write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh)

{
  GatewaySelector *f = (GatewaySelector *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case 0: {    //is_gateway
    bool b;
    if (!cp_bool(s, &b)) 
      return errh->error("is_gateway parameter must be boolean");
    f->_is_gw = b;
    break;
  }
  case 1: { //ignore_add
    IPAddress ip;
    if (!cp_ip_address(s, &ip)) {
      return errh->error("ignore_add parameter must be IPAddress");
    }
    f->_ignore.insert(ip, ip);
    break;
  }

  case 2: { //ignore_del
    IPAddress ip;
    if (!cp_ip_address(s, &ip)) {
      return errh->error("ignore_del parameter must be IPAddress");
    }
    f->_ignore.remove(ip);
    break;
  }

  case 3: { //ignore_clear
    f->_ignore.clear();
    break;
  }

  case 4: { //allow_add
    IPAddress ip;
    if (!cp_ip_address(s, &ip)) {
      return errh->error("allow_add parameter must be IPAddress");
    }
    f->_allow.insert(ip, ip);
    break;
  }

  case 5: { //allow_del
    IPAddress ip;
    if (!cp_ip_address(s, &ip)) {
      return errh->error("allow_del parameter must be IPAddress");
    }
    f->_allow.remove(ip);
    break;
  }

  case 6: { //allow_clear
    f->_allow.clear();
    break;
  }
    
  }
    return 0;

}
void
GatewaySelector::add_handlers()
{
  add_read_handler("is_gateway", read_param, (void *) 0);
  add_read_handler("gateway_stats", read_param, (void *) 1);
  add_read_handler("ignore", read_param, (void *) 2);
  add_read_handler("allow", read_param, (void *) 3);
  
  add_write_handler("is_gateway", write_param, (void *) 0);
  add_write_handler("ignore_add", write_param, (void *) 1);
  add_write_handler("ignore_del", write_param, (void *) 2);
  add_write_handler("ignore_clear", write_param, (void *) 3);
  add_write_handler("allow_add", write_param, (void *) 4);
  add_write_handler("allow_del", write_param, (void *) 5);
  add_write_handler("allow_clear", write_param, (void *) 6);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<GatewaySelector::IPAddress>;
template class DEQueue<GatewaySelector::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(GatewaySelector)

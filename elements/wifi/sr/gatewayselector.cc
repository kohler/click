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
#include "gatewayselector.hh"

CLICK_DECLS

GatewaySelector::GatewaySelector()
  :  Element(1,1),
     _ip(),
     _en(),
     _et(0),
     _link_table(0),
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
                    cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (!_link_table) 
    return errh->error("LT not specified");


  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  _gw_expire.tv_usec = 0;
  _gw_expire.tv_sec = _period*4;

  struct timeval now;
  click_gettimeofday(&now);

  return ret;
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
  pk->set_num_links(0);
  pk->set_link_node(0,_ip);
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

bool
GatewaySelector::update_link(IPAddress from, IPAddress to, uint32_t seq, 
			     uint32_t metric) {
  if (_link_table && !_link_table->update_link(from, to, seq, 0, metric)) {
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

  s->_forwarded = true;
  _link_table->dijkstra(false);
  IPAddress src = s->_gw;
  Path best = _link_table->best_route(src, false);
  bool best_valid = _link_table->valid_route(best);
  
  if (!best_valid) {
    click_chatter("%{element} :: %s :: invalid route from src %s\n",
		  this,
		  __func__,
		  src.s().cc());
    return;
  }

  int links = best.size() - 1;

  int len = srpacket::len_wo_data(links);
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
  pk->set_num_links(links);

  for(int i=0; i < links; i++) {
    if (_link_table) {
      pk->set_link(i,
		   best[i], best[i+1],
		   _link_table->get_link_metric(best[i], best[i+1]),
		   _link_table->get_link_metric(best[i+1], best[i]),
		   _link_table->get_link_seq(best[i], best[i+1]),
		   _link_table->get_link_age(best[i], best[i+1]));
    } else {
      pk->set_link(i,
		   best[i], best[i+1],
		   0,0,0,0);
    }
  }

  send(p);
}


IPAddress
GatewaySelector::best_gateway() 
{
  IPAddress best_gw = IPAddress();
  int best_metric = 0;
  struct timeval expire;
  struct timeval now;
  
  _link_table->dijkstra(false);
  click_gettimeofday(&now);
  
  for(GWIter iter = _gateways.begin(); iter; iter++) {
    GWInfo nfo = iter.value();
    timeradd(&nfo._last_update, &_gw_expire, &expire);
    Path p = _link_table->best_route(nfo._ip, false);
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

  if (pk->_version != _sr_version) {
    click_chatter("%{element} bad sr version %d vs %d\n",
		  this,
		  pk->_version,
		  _sr_version);
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
    
  for(int i = 0; i < pk->num_links(); i++) {
    IPAddress a = pk->get_link_node(i);
    IPAddress b = pk->get_link_node(i+1);
    uint32_t fwd_m = pk->get_link_fwd(i);
    uint32_t rev_m = pk->get_link_rev(i);
    uint32_t seq = pk->get_link_seq(i);
    
    if (a == _ip || b == _ip || 
	!fwd_m || !rev_m || !seq) {
      p_in->kill();
      return;
    }

    /* don't update my immediate neighbor. see below */
    if (fwd_m && !update_link(a,b,seq,fwd_m)) {
      click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		    this,
		    a.s().cc(),
		    fwd_m,
		    b.s().cc());
    }
    if (rev_m && !update_link(b,a,seq,rev_m)) {
      click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		    this,
		    b.s().cc(),
		    rev_m,
		    a.s().cc());
    }

  
  }

  IPAddress neighbor = pk->get_link_node(pk->num_links());
  if (_arp_table) {
    _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  }
  
  IPAddress gw = pk->_qdst;
  sr_assert(gw);
  GWInfo *nfo = _gateways.findp(gw);
  if (!nfo) {
    _gateways.insert(gw, GWInfo());
    nfo = _gateways.findp(gw);
  }
  nfo->_ip = gw;
  click_gettimeofday(&nfo->_last_update);

  if (_is_gw) {
    p_in->kill();
    return;
  }

  int si = 0;
  uint32_t seq = pk->seq();
  for(si = 0; si < _seen.size(); si++){
    if(gw == _seen[si]._gw && seq == _seen[si]._seq){
      _seen[si]._count++;
      p_in->kill();
      return;
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


  /* schedule timer */
  int delay_time = (random() % 2000) + 1;
  sr_assert(delay_time > 0);
  
  struct timeval delay;
  delay.tv_sec = 0;
  delay.tv_usec = delay_time*1000;
  timeradd(&_seen[si]._when, &delay, &_seen[si]._to_send);
  _seen[si]._forwarded = false;
  Timer *t = new Timer(static_forward_ad_hook, (void *) this);
  t->initialize(this);
  
  t->schedule_after_ms(delay_time);
  

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
      
      Path p = _link_table->best_route(nfo._ip, false);
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

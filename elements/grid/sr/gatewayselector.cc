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
#include "gatewayselector.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/sr/srcrstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#ifndef gatewayselector_assert
#define gatewayselector_assert(e) ((e) ? (void) 0 : gatewayselector_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


GatewaySelector::GatewaySelector()
  :  Element(1,1),
     _warmup(0),
     _srcr_stat(0),
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
		    cpUnsigned, "Ethernet encapsulation type", &_et,
                    cpIPAddress, "IP address", &_ip,
		    cpEtherAddress, "EtherAddress", &_en,
		    cpElement, "LinkTable element", &_link_table,
		    cpElement, "ARPTable element", &_arp_table,
                    cpKeywords,
		    "PERIOD", cpUnsigned, "Ad broadcast period (secs)", &_period,
		    "GW", cpBool, "Gateway", &_is_gw,
		    "SS", cpElement, "SrcrStat element", &_srcr_stat,
                    0);

  if (_link_table && _link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_srcr_stat && _srcr_stat->cast("SrcrStat") == 0) 
    return errh->error("SS element is not a SrcrStat");
  if (_arp_table && _arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");

  _gw_expire.tv_usec = 0;
  _gw_expire.tv_sec = _period*4;

  struct timeval now;
  click_gettimeofday(&now);

  struct timeval warmup_forward;
  warmup_forward.tv_sec = _period;
  warmup_forward.tv_usec = 0;
  timeradd(&now, &warmup_forward, &_warmup_forward_expire);

  struct timeval warmup;
  warmup.tv_sec = _period*2;
  warmup.tv_usec = 0;
  timeradd(&now, &warmup, &_warmup_expire);


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
  int len = sr_pkt::len_wo_data(1);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _srcr_version;
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
GatewaySelector::get_metric(IPAddress other)
{
  int metric = 9999;
  gatewayselector_assert(other);
  if (_srcr_stat) {
    metric = _srcr_stat->get_etx(other);
  }
  update_link(_ip, other, metric);
  return metric;
}

void
GatewaySelector::update_link(IPAddress from, IPAddress to, int metric) {
  if (_link_table) {
    _link_table->update_link(from, to, metric);
    _link_table->update_link(to, from, metric);
  }
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

  gatewayselector_assert(s->_hops.size() == s->_metrics.size()+1);

  int len = sr_pkt::len_wo_data(nhops);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _srcr_version;
  pk->_type = PT_GATEWAY;
  pk->_flags = 0;
  pk->_qdst = s->_gw;
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
	((!best_metric) || best_metric > metric)) {
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
  struct sr_pkt *pk = (struct sr_pkt *) (eh+1);
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
  neighbor = pk->get_hop(pk->num_hops()-1);

  _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  update_link(_ip, neighbor, get_metric(neighbor));


  Vector<IPAddress> hops;
  Vector<int> metrics;

  int metric = 0;
  for(int i = 0; i < pk->num_hops(); i++) {
    IPAddress hop = IPAddress(pk->get_hop(i));
    if (i != pk->num_hops() - 1) {
      metrics.push_back(pk->get_metric(i));
      metric += pk->get_metric(i);
    }
    hops.push_back(hop);
    if (hop == _ip) {
      /* I'm already in this route! */
      p_in->kill();
      return;
    }
  }
  /* also get the metric from the neighbor */
  int m = get_metric(pk->get_hop(pk->num_hops()-1));
  metric += m;
  metrics.push_back(m);
  hops.push_back(_ip);

  IPAddress gw = pk->_qdst;
  gatewayselector_assert(gw);
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
    _seen.push_back(Seen(gw, seq, 0));
  }
  _seen[si]._count++;
  if (_seen[si]._metric && _seen[si]._metric <= metric) {
    /* the metric is worse that what we've seen*/
    p_in->kill();
    return;
  }

  _seen[si]._metric = metric;

  click_gettimeofday(&_seen[si]._when);
  
  _seen[si]._hops = hops;
  _seen[si]._metrics = metrics;

  if (_is_gw) {
    p_in->kill();
    return;
  }


  if (timercmp(&_seen[si]._when, &_seen[si]._to_send, <)) {
    /* a timer has already been scheduled */
    p_in->kill();
    return;
  } else {
    /* schedule timer */
    int delay_time = (random() % 750) + 1;
    gatewayselector_assert(delay_time > 0);
    
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
GatewaySelector::static_print_gateway_stats(Element *f, void *)
{
  GatewaySelector *d = (GatewaySelector *) f;
  return d->print_gateway_stats();
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
GatewaySelector::static_print_is_gateway(Element *f, void *)
{
  GatewaySelector *d = (GatewaySelector *) f;
  return d->print_is_gateway();
}

String
GatewaySelector::print_is_gateway()
{
  
  if (_is_gw) {
    return "true\n";
  }
  return "false\n";
}
int
GatewaySelector::static_write_is_gateway(const String &arg, Element *el,
			     void *, ErrorHandler *errh)
{
  GatewaySelector *d = (GatewaySelector *) el;
  bool b;
  if (!cp_bool(arg, &b)) {
    return errh->error("arg must be bool");
  }

  d->write_is_gateway(b);
  return 0;
}

void
GatewaySelector::write_is_gateway(bool b)
{
  _is_gw = b;
}



void
GatewaySelector::add_handlers()
{
  add_read_handler("gateway_stats", static_print_gateway_stats, 0);
  add_read_handler("print_is_gateway", static_print_is_gateway, 0);
  add_write_handler("write_is_gateway", static_write_is_gateway, 0);
}

void
GatewaySelector::gatewayselector_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("GatewaySelector %s assertion \"%s\" failed: file %s, line %d",
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
template class Vector<GatewaySelector::IPAddress>;
template class DEQueue<GatewaySelector::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(GatewaySelector)

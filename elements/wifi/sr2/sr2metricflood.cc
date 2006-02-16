/*
 * SR2MetricFlood.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussr2metricfloods Institute of Technology
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
#include "sr2metricflood.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "sr2packet.hh"
CLICK_DECLS



SR2MetricFlood::SR2MetricFlood()
  :  _ip(),
     _en(),
     _et(0),
     _link_table(0),
     _arp_table(0)
{

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

SR2MetricFlood::~SR2MetricFlood()
{
}

int
SR2MetricFlood::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsignedShort, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    /* below not required */
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    "DEBUG", cpBool, "Debug", &_debug,
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
    return errh->error("ARPTable element is not a ARPTable");

  return ret;
}

int
SR2MetricFlood::initialize (ErrorHandler *)
{
  return 0;
}

IPAddress
SR2MetricFlood::get_random_neighbor()
{
  if (!_neighbors_v.size()) {
    return IPAddress();
  }
  int ndx = random() % _neighbors_v.size();
  return _neighbors_v[ndx];

}


bool
SR2MetricFlood::update_link(IPAddress from, IPAddress to, 
			      uint32_t seq, uint32_t age,
			      uint32_t metric) {
  if (!from || !to || !metric) {
    return false;
  }
  if (_link_table && !_link_table->update_link(from, to, seq, age, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().c_str(),
		  metric,
		  to.s().c_str());
    return false;
  }
  return true;
}

void
SR2MetricFlood::forward_query_hook() 
{
  Timestamp now = Timestamp::now();
  for (int x = 0; x < _seen.size(); x++) {
    if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
      forward_query(&_seen[x]);
    }
  }
}
void
SR2MetricFlood::forward_query(Seen *s)
{

  s->_forwarded = true;
  _link_table->dijkstra(false);

  Packet *p_in = s->_p;
  s->_p = 0;

  if (!p_in) {
    return;
  }

  if (0) {
    StringAccum sa;
    sa << Timestamp::now() - s->_when;
    click_chatter("%{element} :: %s :: waited %s\n",
		  this,
		  __func__,
		  sa.take_string().c_str());
  }

  IPAddress src = s->_src;
  Path best = _link_table->best_route(src, false);
  bool best_valid = _link_table->valid_route(best);

  if (!best_valid) {
    click_chatter("%{element} :: %s :: invalid route from src %s\n",
		  this,
		  __func__,
		  src.s().c_str());
    p_in->kill();
    return;
  }

  if (_debug) {
     click_chatter("%{element}: forward_query %s -> %s %d\n", 
		  this,
		  s->_src.s().c_str(),
		  s->_dst.s().c_str(),
		  s->_seq);
  }

  int links = best.size() - 1;

  click_ether *eh = (click_ether *) p_in->data();
  struct sr2packet *pk = (struct sr2packet *) (eh+1);

  int extra = pk->hlen_wo_data() + sizeof(click_ether);
  p_in->pull(extra);

  int dlen = p_in->length();
  extra = sr2packet::len_wo_data(links) + sizeof(click_ether);
  WritablePacket *p = p_in->push(extra);

  if (p == 0)
    return;
  eh = (click_ether *) p->data();
  pk = (struct sr2packet *) (eh+1);
  memset(pk, '\0', extra);
  pk->_version = _sr2_version;
  pk->_type = SR2_PT_DATA;
  pk->unset_flag(~0);
  pk->set_qdst(s->_dst);
  pk->set_seq(s->_seq);
  pk->set_num_links(links);
  pk->set_data_len(dlen);

  for (int i = 0; i < links; i++) {
    pk->set_link(i,
		 best[i], best[i+1],
		 _link_table->get_link_metric(best[i], best[i+1]),
		 _link_table->get_link_metric(best[i+1], best[i]),
		 _link_table->get_link_seq(best[i], best[i+1]),
		 _link_table->get_link_age(best[i], best[i+1]));
  }
	       
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);
}


void 
SR2MetricFlood::start_flood(Packet *p_in) {
  IPAddress qdst = p_in->dst_ip_anno();
  int dlen = p_in->length();
  unsigned extra = sr2packet::len_wo_data(0) + sizeof(click_ether);
  WritablePacket *p = p_in->push(extra);
  if (p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);

  struct sr2packet *pk = (struct sr2packet *) (eh+1);
  memset(pk, '\0', sr2packet::len_wo_data(0));
  pk->_version = _sr2_version;
  pk->_type = SR2_PT_DATA;
  pk->unset_flag(~0);
  pk->set_qdst(qdst);
  pk->set_seq(++_seq);
  pk->set_num_links(0);
  pk->set_link_node(0,_ip);
  pk->set_data_len(dlen);


  if (_debug) {
    click_chatter("%{element} start_query %s %d\n",
		  this, qdst.s().c_str(), _seq);

  }

  output(0).push(p);
}

void 
SR2MetricFlood::process_flood(Packet *p_in) {
  click_ether *eh = (click_ether *) p_in->data();
  struct sr2packet *pk = (struct sr2packet *) (eh+1);
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

  /* update the metrics from the packet */
  for(int i = 0; i < pk->num_links(); i++) {
    IPAddress a = pk->get_link_node(i);
    IPAddress b = pk->get_link_node(i+1);
    uint32_t fwd_m = pk->get_link_fwd(i);
    uint32_t rev_m = pk->get_link_fwd(i);
    uint32_t seq = pk->get_link_seq(i);
    uint32_t age = pk->get_link_age(i);

    if (fwd_m && !update_link(a, b, seq, age, fwd_m)) {
      click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		    this,
		    a.s().c_str(),
		    fwd_m,
		    b.s().c_str());
    }
    if (rev_m && !update_link(b, a, seq, age, rev_m)) {
      click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		    this,
		    b.s().c_str(),
		    rev_m,
		    a.s().c_str());
    }
  }
  
  
  IPAddress neighbor = pk->get_link_node(pk->num_links());
  if (!neighbor) {
	  p_in->kill();
	  return;
  }
  
  if (!_neighbors.findp(neighbor)) {
    _neighbors.insert(neighbor, true);
    _neighbors_v.push_back(neighbor);
  }
  
  if (_arp_table) {
    _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  }
  
  IPAddress src = pk->get_link_node(0);
  IPAddress dst = pk->get_qdst();
  u_long seq = pk->seq();

  int si = 0;
  
  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq) {
      _seen[si]._count++;
      p_in->kill();
      return;
    }
  }
  
  if (_seen.size() >= 100) {
    _seen.pop_front();
  }
  _seen.push_back(Seen(src, dst, seq, 0, 0));
  si = _seen.size() - 1;
  
  _seen[si]._count++;
  _seen[si]._when = Timestamp::now();
  _seen[si]._p = 0;

  if (dst == _ip) {
    /* don't forward queries for me */
    /* just spit them out the output */
    output(1).push(p_in);
    return;
  }

  _seen[si]._p = p_in->clone();
  
  /* schedule timer */
  int delay_time = random() % 1750 + 1;
  
  _seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay_time);
  _seen[si]._forwarded = false;
  Timer *t = new Timer(static_forward_query_hook, (void *) this);
  t->initialize(this);
  t->schedule_after_msec(delay_time);


  output(1).push(p_in);
  return;
  
  


}

void
SR2MetricFlood::push(int port, Packet *p_in)
{
  if (port == 0) {
    process_flood(p_in);
  } else if (port == 1) {
    start_flood(p_in);
  } else {
    p_in->kill();
    return;
  }
}


enum {H_DEBUG, H_IP, H_CLEAR, H_FLOODS};

static String 
SR2MetricFlood_read_param(Element *e, void *thunk)
{
  SR2MetricFlood *td = (SR2MetricFlood *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_IP:
    return td->_ip.s() + "\n";
  case H_FLOODS: {
	  StringAccum sa;
	  int x;
	  for (x = 0; x < td->_seen.size(); x++) {
		  sa << "src " << td->_seen[x]._src;
		  sa << " dst " << td->_seen[x]._dst;
		  sa << " seq " << td->_seen[x]._seq;
		  sa << " count " << td->_seen[x]._count;
		  sa << " forwarded " << td->_seen[x]._forwarded;
		  sa << "\n";
	  }
	  return sa.take_string();
  }
  default:
    return String();
  }
}
static int 
SR2MetricFlood_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SR2MetricFlood *f = (SR2MetricFlood *)e;
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
SR2MetricFlood::add_handlers()
{
  add_read_handler("debug", SR2MetricFlood_read_param, (void *) H_DEBUG);
  add_read_handler("ip", SR2MetricFlood_read_param, (void *) H_IP);
  add_read_handler("floods", SR2MetricFlood_read_param, (void *) H_FLOODS);

  add_write_handler("debug", SR2MetricFlood_write_param, (void *) H_DEBUG);
  add_write_handler("clear", SR2MetricFlood_write_param, (void *) H_CLEAR);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SR2MetricFlood::IPAddress>;
template class DEQueue<SR2MetricFlood::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2MetricFlood)

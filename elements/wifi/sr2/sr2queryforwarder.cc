/*
 * SR2QueryForwarder.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussr2queryforwarders Institute of Technology
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
#include "sr2queryforwarder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "sr2packet.hh"
CLICK_DECLS



SR2QueryForwarder::SR2QueryForwarder()
  :  _ip(),
     _en(),
     _et(0),
     _link_table(0),
     _arp_table(0)
{

  MaxSeen = 200;
  MaxHops = 30;

  // Pick a starting sequence number that we have not used before.
  _seq = Timestamp::now().usec();

  _query_wait = Timestamp(5, 0);


  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

SR2QueryForwarder::~SR2QueryForwarder()
{
}

int
SR2QueryForwarder::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsignedShort, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    /* below not required */
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
  if (!_arp_table) 
    return errh->error("ARPTable not specified");


  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");
  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not a ARPTable");

  return ret;
}

int
SR2QueryForwarder::initialize (ErrorHandler *)
{
  return 0;
}

IPAddress
SR2QueryForwarder::get_random_neighbor()
{
  if (!_neighbors_v.size()) {
    return IPAddress();
  }
  int ndx = random() % _neighbors_v.size();
  return _neighbors_v[ndx];

}


bool
SR2QueryForwarder::update_link(IPAddress from, IPAddress to, 
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

// Continue flooding a query by broadcast.
// Maintain a list of querys we've already seen.
void
SR2QueryForwarder::process_query(struct sr2packet *pk1)
{
  IPAddress src(pk1->get_link_node(0));
  IPAddress dst = pk1->get_qdst();
  u_long seq = pk1->seq();

  if (dst == _ip) {
    /* don't forward queries for me */
    return;
  }

  for(int i = 0; i < pk1->num_links(); i++) {
    IPAddress hop = IPAddress(pk1->get_link_node(i));
    if (hop == _ip) {
      /* I'm already in this route! */
      return;
    }
  }

  
  int si = 0;
  
  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq) {
      _seen[si]._count++;
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

  
  /* schedule timer */
  int delay_time = random() % 1750 + 1;
  
  _seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay_time);
  _seen[si]._forwarded = false;
  Timer *t = new Timer(static_forward_query_hook, (void *) this);
  t->initialize(this);
  t->schedule_after_msec(delay_time);

}
void
SR2QueryForwarder::forward_query_hook() 
{
  Timestamp now = Timestamp::now();
  for (int x = 0; x < _seen.size(); x++) {
    if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
      forward_query(&_seen[x]);
    }
  }
}
void
SR2QueryForwarder::forward_query(Seen *s)
{

  s->_forwarded = true;
  _link_table->dijkstra(false);
  if (0) {
    StringAccum sa;
    sa << (Timestamp::now() - s->_when);
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
    return;
  }

  if (_debug) {
    click_chatter("%{element}: forward_query %s -> %s\n", 
		  this,
		  s->_src.s().c_str(),
		  s->_dst.s().c_str());
  }

  int links = best.size() - 1;

  //click_chatter("forward query called");
  int len = sr2packet::len_wo_data(links);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr2packet *pk = (struct sr2packet *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr2_version;
  pk->_type = SR2_PT_QUERY;
  pk->unset_flag(~0);
  pk->set_qdst(s->_dst);
  pk->set_seq(s->_seq);
  pk->set_num_links(links);

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
SR2QueryForwarder::push(int, Packet *p_in)
{

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

  u_char type = pk->_type;

  if (type != SR2_PT_QUERY) {
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
  
  _arp_table->insert(neighbor, EtherAddress(eh->ether_shost));
  process_query(pk);
  
  p_in->kill();
  return;
  
  

}

enum {H_DEBUG, H_IP, H_CLEAR, H_QUERIES};

static String 
SR2QueryForwarder_read_param(Element *e, void *thunk)
{
  SR2QueryForwarder *td = (SR2QueryForwarder *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_IP:
    return td->_ip.s() + "\n";
  case H_QUERIES: {
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
SR2QueryForwarder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SR2QueryForwarder *f = (SR2QueryForwarder *)e;
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
SR2QueryForwarder::add_handlers()
{
  add_read_handler("debug", SR2QueryForwarder_read_param, (void *) H_DEBUG);
  add_read_handler("ip", SR2QueryForwarder_read_param, (void *) H_IP);
  add_read_handler("queries", SR2QueryForwarder_read_param, (void *) H_QUERIES);

  add_write_handler("debug", SR2QueryForwarder_write_param, (void *) H_DEBUG);
  add_write_handler("clear", SR2QueryForwarder_write_param, (void *) H_CLEAR);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SR2QueryForwarder::IPAddress>;
template class DEQueue<SR2QueryForwarder::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2QueryForwarder)

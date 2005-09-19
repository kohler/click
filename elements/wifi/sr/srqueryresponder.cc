/*
 * SRQueryResponder.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrqueryresponders Institute of Technology
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
#include "srqueryresponder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
CLICK_DECLS



SRQueryResponder::SRQueryResponder()
  :  _ip(),
     _en(),
     _et(0),
     _link_table(0),
     _arp_table(0)
{
}

SRQueryResponder::~SRQueryResponder()
{
}

int
SRQueryResponder::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
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
SRQueryResponder::initialize (ErrorHandler *)
{

  return 0;
}

// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
SRQueryResponder::send(WritablePacket *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  int next = pk->next();
  IPAddress next_ip = pk->get_link_node(next);
  EtherAddress eth_dest = _arp_table->lookup(next_ip);

  sr_assert(next_ip != _ip);
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memcpy(eh->ether_dhost, eth_dest.data(), 6);

  output(0).push(p);
}


bool
SRQueryResponder::update_link(IPAddress from, IPAddress to, uint32_t seq, int metric) {
  if (_link_table && !_link_table->update_link(from, to, seq, 0, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().c_str(),
		  metric,
		  to.s().c_str());
    return false;
  }
  return true;
}

// Continue unicasting a reply packet.
void
SRQueryResponder::forward_reply(struct srpacket *pk1)
{
  u_char type = pk1->_type;
  sr_assert(type == PT_REPLY);

  _link_table->dijkstra(true);
  if (_debug) {
    click_chatter("%{element}: forward_reply %s <- %s\n", 
		  this,
		  pk1->get_link_node(0).s().c_str(),
		  IPAddress(pk1->_qdst).s().c_str());
  }
  if(pk1->next() >= pk1->num_links()) {
    click_chatter("%{element} forward_reply strange next=%d, nhops=%d", 
		  this,
		  pk1->next(), 
		  pk1->num_links());
    return;
  }

  Path fwd;
  Path rev;
  for (int i = 0; i < pk1->num_links(); i++) {
    fwd.push_back(pk1->get_link_node(i));
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

void SRQueryResponder::start_reply(IPAddress src, IPAddress qdst, uint32_t seq)
{
  _link_table->dijkstra(false);
  Path best = _link_table->best_route(src, false);
  bool best_valid = _link_table->valid_route(best);

  
  int si = 0;
  
  for(si = 0; si < _seen.size(); si++){
    if(src == _seen[si]._src && seq == _seen[si]._seq) {
      break;
    }
  }

  if (si == _seen.size()) {
    if (_seen.size() >= 100) {
      _seen.pop_front();
    }
    _seen.push_back(Seen(src, qdst, seq));
    si = _seen.size() - 1;
  }

  if (best == _seen[si].last_path_response) {
    /*
     * only send replies if the "best" path is different
     * from the last reply
     */
    return;
  }

  _seen[si]._src = src;
  _seen[si]._dst = qdst;
  _seen[si]._seq = seq;
  _seen[si].last_path_response = best;
  
  if (!best_valid) {
    click_chatter("%{element} :: %s :: invalid route for src %s: %s\n",
		  this,
		  __func__,
		  src.s().c_str(),
		  path_to_string(best).c_str());
    return;
  }
  int links = best.size() - 1;
  int len = srpacket::len_wo_data(links);
  if (_debug) {
    click_chatter("%{element}: start_reply %s <- %s\n",
		  this,
		  src.s().c_str(),
		  qdst.s().c_str());
  }
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk_out = (struct srpacket *) (eh+1);
  memset(pk_out, '\0', len);


  pk_out->_version = _sr_version;
  pk_out->_type = PT_REPLY;
  pk_out->_flags = 0;
  pk_out->set_seq(seq);
  pk_out->set_num_links(links);
  pk_out->set_next(links-1);
  pk_out->_qdst = qdst;

  
  for (int i = 0; i < links; i++) {
    pk_out->set_link(i,
		     best[i], best[i+1],
		     _link_table->get_link_metric(best[i], best[i+1]),
		     _link_table->get_link_metric(best[i+1], best[i]),
		     _link_table->get_link_seq(best[i], best[i+1]),
		     _link_table->get_link_age(best[i], best[i+1]));
  }
  
  send(p);
}

// Got a reply packet whose ultimate consumer is us.
// Make a routing table entry if appropriate.
void
SRQueryResponder::got_reply(struct srpacket *pk)
{

  IPAddress dst = IPAddress(pk->_qdst);
  sr_assert(dst);
  if (_debug) {
    click_chatter("%{element}: got_reply %s <- %s\n", 
		  this,
		  _ip.s().c_str(),
		  dst.s().c_str());
  }
  _link_table->dijkstra(true);

}


void
SRQueryResponder::push(int, Packet *p_in)
{

  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  if (EtherAddress(eh->ether_shost) == _en) {
    click_chatter("%{element}: packet from me",
		  this);
    p_in->kill();
    return;
  }
  
  u_char type = pk->_type;
  IPAddress dst = IPAddress(pk->_qdst);
  
  if (type != PT_REPLY) {
    if (dst == _ip) {
      start_reply(pk->get_link_node(0), pk->_qdst, pk->seq());
    }
    p_in->kill();
    return;
  }

  if (eh->ether_type != htons(_et)) {
    click_chatter("%{element}: bad ether_type %04x",
		  this,
		  ntohs(eh->ether_type));
    p_in->kill();
    return;
  }

  if(pk->get_link_node(pk->next()) != _ip){
    // it's not for me. these are supposed to be unicast,
    // so how did this get to me?
    click_chatter("%{element}: reply not for me %d/%d %s",
		  this,
		  pk->next(),
		  pk->num_links(),
		  pk->get_link_node(pk->next()).s().c_str());
    p_in->kill();
    return;
  }
  

    /* update the metrics from the packet */
  for(int i = 0; i < pk->num_links(); i++) {
    IPAddress a = pk->get_link_node(i);
    IPAddress b = pk->get_link_node(i+1);
    int fwd_m = pk->get_link_fwd(i);
    int rev_m = pk->get_link_fwd(i);
    uint32_t seq = pk->get_link_seq(i);
    /* don't update my immediate neighbor. see below */
    if (fwd_m && !update_link(a,b,seq,fwd_m)) {
      click_chatter("%{element} couldn't update fwd_m %s > %d > %s\n",
		    this,
		    a.s().c_str(),
		    fwd_m,
		    b.s().c_str());
    }
    if (rev_m && !update_link(b,a,seq,rev_m)) {
      click_chatter("%{element} couldn't update rev_m %s > %d > %s\n",
		    this,
		    b.s().c_str(),
		    rev_m,
		    a.s().c_str());
    }
  }
  
  
  IPAddress neighbor = pk->get_link_node(pk->num_links());
  sr_assert(neighbor);
  
  if(pk->next() == 0){
    // I'm the ultimate consumer of this reply. Add to routing tbl.
    got_reply(pk);
  } else {
    // Forward the reply.
    forward_reply(pk);
  }
  p_in->kill();
  return;
    
}

enum {H_DEBUG, H_IP};

static String 
SRQueryResponder_read_param(Element *e, void *thunk)
{
  SRQueryResponder *td = (SRQueryResponder *)e;
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
SRQueryResponder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SRQueryResponder *f = (SRQueryResponder *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}
void
SRQueryResponder::add_handlers()
{
  add_read_handler("debug", SRQueryResponder_read_param, (void *) H_DEBUG);
  add_read_handler("ip", SRQueryResponder_read_param, (void *) H_IP);

  add_write_handler("debug", SRQueryResponder_write_param, (void *) H_DEBUG);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SRQueryResponder::IPAddress>;
template class DEQueue<SRQueryResponder::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRQueryResponder)

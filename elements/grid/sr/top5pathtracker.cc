/*
 * Top5PathTracker.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachuseesrs Institute of Technology
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "srforwarder.hh"
#include "srcrstat.hh"
#include "top5pathtracker.hh"


CLICK_DECLS

#ifndef top5pathtracker_assert
#define top5pathtracker_assert(e) ((e) ? (void) 0 : top5pathtracker_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


Top5PathTracker::Top5PathTracker()
  :  Element(1,2),
     _et(0),
     _ip(),
     _en(),
     _arp_table(0)
{
  MOD_INC_USE_COUNT;
}

Top5PathTracker::~Top5PathTracker()
{
  MOD_DEC_USE_COUNT;
}

int
Top5PathTracker::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  int path_duration = 10000;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "PATH_DURATION", cpUnsigned, "Timer per path in ms", &path_duration,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (!_arp_table) 
    return errh->error("ARPTable not specified");
  if (_arp_table->cast("ARPTable") == 0) 
    return errh->error("ARPTable element is not an ARPtable");
  
  timerclear(&_time_per_path);
  /* convert path_duration from ms to a struct timeval */
  _time_per_path.tv_sec = path_duration/1000;
  _time_per_path.tv_usec = (path_duration % 1000) * 1000;
  return ret;
}

Top5PathTracker *
Top5PathTracker::clone () const
{
  return new Top5PathTracker;
}

int
Top5PathTracker::initialize (ErrorHandler *)
{
  return 0;
}


// Send a packet.
// Decides whether to broadcast or unicast according to type.
// Assumes the _next field already points to the next hop.
void
Top5PathTracker::send(WritablePacket *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);

  u_char type = pk->_type;
  if(type == PT_QUERY){
    memset(eh->ether_dhost, 0xff, 6);
  } else if(type == PT_REPLY){
    int next = pk->next();
    EtherAddress eth_dest = _arp_table->lookup(pk->get_hop(next));
    memcpy(eh->ether_dhost, eth_dest.data(), 6);
  } else {
    top5pathtracker_assert(0);
    return;
  }

  output(1).push(p);
}


void
Top5PathTracker::push(int, Packet *p_in)
{

  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  if (pk->_type != PT_DATA) {
    click_chatter("Top5PathTracker %s: got a non-data packet\n",
		  id().cc());
    p_in->kill();
    return;
  }
  

  if (pk->get_hop(pk->num_hops()-1) != _ip) {
    output(0).push(p_in);
    return;
  }

  Path p;
  for(int i = 0; i < pk->num_hops(); i++) {
    p.push_back(pk->get_hop(i));
  }
  
  
  IPAddress src = p[0];

  Src *s = _sources.findp(src);
  if (!s) {
    _sources.insert(src, Src());
    s = _sources.findp(src);
    s->_ip = src;
  }


  if (pk->flag(FLAG_TOP5_REQUEST_RESULT)) {
    if (s->_paths.size() == 0) {
      click_chatter("%s:  request for best route from %s, but can't find any routes!",
		    id().cc(),
		    s->_ip.s().cc());
      output(0).push(p_in);
      return;
    }
    /* find the best route */
    int best_path = 0;
    int best_count = 0;
    for (int x = 0 ; x < s->_paths.size(); x++) {
      if (best_count < s->_count_per_path[x]) {
	best_count = s->_count_per_path[x];
	best_path = x;
      }
    }
    int len = srpacket::len_wo_data(s->_paths[best_path].size());
    
    click_chatter("PathTracker %s: start_best_route_reply %s <- %s\n",
		  id().cc(),
		  s->_ip.s().cc(),
		  _ip.s().cc());
    WritablePacket *p_out = Packet::make(len + sizeof(click_ether));
    if(p_out == 0) {
      output(0).push(p_in);
      p_in->kill();
      return;
    }
  
    click_ether *eh = (click_ether *) p_out->data();
    struct srpacket *pk_out = (struct srpacket *) (eh+1);
    memset(pk_out, '\0', len);


    pk_out->_version = _sr_version;
    pk_out->_type = PT_REPLY;
    pk_out->_flags = 0;
    pk_out->_seq = s->_seq;
    pk_out->set_num_hops(s->_paths[best_path].size());
    pk_out->set_next(s->_paths[best_path].size() - 2);
    pk_out->_qdst = _ip;
    
    for (int x = 0; x < s->_paths[best_path].size(); x++) {
      IPAddress ip = s->_paths[best_path][x];
      pk_out->set_hop(x, ip);
    }
    pk_out->set_flag(FLAG_TOP5_BEST_ROUTE);
    send(p_out);
    output(0).push(p_in);
    return;
  }

  if (pk->seq() != s->_seq) {
    s->_seq = pk->seq();
    click_chatter("PathTracker %s: clearing path; new seq %d\n",
		  id().cc(),
		  s->_seq);
    s->_paths.clear();
    s->_count_per_path.clear();
    s->_first_received.clear();
  }

  struct timeval now;
  click_gettimeofday(&now);
  int current_path = 0;
  for (current_path = 0; 
       current_path < s->_paths.size();
       current_path++) {

    if (p == s->_paths[current_path]) {
      break;
    }
  }


  if (current_path == s->_paths.size()) {
    click_chatter("PathTracker %s: found new path %s\n",
		  id().cc(),
		  path_to_string(p).cc());
    s->_paths.push_back(p);
    s->_count_per_path.push_back(0);
    s->_first_received.push_back(now);
  }

  struct timeval expire;
  timeradd(&_time_per_path, &s->_first_received[current_path], &expire);
  if (timercmp(&now, &expire, <)) {
    s->_count_per_path[current_path] = s->_count_per_path[current_path]+1;
    click_chatter("PathTracker %s: got %d packets on path %d, %s\n",
		  id().cc(),
		  s->_count_per_path[current_path],
		  current_path,
		  path_to_string(s->_paths[current_path]).cc());
  }

  output(0).push(p_in);
  return;

}


String
Top5PathTracker::static_print_stats(Element *f, void *)
{
  Top5PathTracker *d = (Top5PathTracker *) f;
  return d->print_stats();
}

String
Top5PathTracker::print_stats()
{
  
  return "";
}

int
Top5PathTracker::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  Top5PathTracker *n = (Top5PathTracker *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
Top5PathTracker::clear() 
{
}

void
Top5PathTracker::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("clear", static_clear, 0);
}

void
Top5PathTracker::top5pathtracker_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("Top5PathTracker %s assertion \"%s\" failed: file %s, line %d",
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
template class Vector<Top5PathTracker::IPAddress>;
template class DEQueue<Top5PathTracker::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(Top5PathTracker)

/*
 * SR1GatewayResponder.{cc,hh} -- DSR implementation
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
#include "sr1gatewayresponder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
CLICK_DECLS



SR1GatewayResponder::SR1GatewayResponder()
  :  _ip(),
     _en(),
     _et(0),
     _link_table(0),
     _arp_table(0),
     _timer(this)
{
}

SR1GatewayResponder::~SR1GatewayResponder()
{
}

int
SR1GatewayResponder::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _period = 15;
  _debug = false;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    "ARP", cpElement, "ARPTable element", &_arp_table,
		    "PERIOD", cpUnsigned, "Period", &_period,
		    "SEL", cpElement, "GatewaySelector element", &_gw_sel,
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
SR1GatewayResponder::initialize (ErrorHandler *)
{
	_timer.initialize (this);
	_timer.schedule_now ();
	
	return 0;
}

void
SR1GatewayResponder::run_timer(Timer *)
{
	if (!_gw_sel->is_gateway()) {
		IPAddress gateway = _gw_sel->best_gateway();
		_link_table->dijkstra(false);
		Path best = _link_table->best_route(gateway, false);
		bool best_valid = _link_table->valid_route(best);
		
		if (best_valid) {
			int links = best.size() - 1;
			int len = srpacket::len_wo_data(links);
			if (_debug) {
				click_chatter("%{element}: start_reply %s <- %s\n",
					      this,
					      gateway.s().c_str(),
					      _ip.s().c_str());
			}
			WritablePacket *p = Packet::make(len + sizeof(click_ether));
			if(p == 0)
				return;
			struct srpacket *pk = (struct srpacket *) (p->data() + sizeof(click_ether));
			memset(pk, '\0', len);
			
			pk->_version = _sr_version;
			pk->_type = PT_REPLY;
			pk->_flags = 0;
			pk->set_seq(0);
			pk->set_num_links(links);
			pk->set_next(links-1);
			pk->_qdst = _ip;
			
			for (int i = 0; i < links; i++) {
				pk->set_link(i,
					     best[i], best[i+1],
					     _link_table->get_link_metric(best[i], best[i+1]),
					     _link_table->get_link_metric(best[i+1], best[i]),
					     _link_table->get_link_seq(best[i], best[i+1]),
					     _link_table->get_link_age(best[i], best[i+1]));
			}
			
			click_ether *eh = (click_ether *) p->data();
			int next = pk->next();
			IPAddress next_ip = pk->get_link_node(next);
			EtherAddress eth_dest = _arp_table->lookup(next_ip);
			
			sr_assert(next_ip != _ip);
			eh->ether_type = htons(_et);
			memcpy(eh->ether_shost, _en.data(), 6);
			memcpy(eh->ether_dhost, eth_dest.data(), 6);
			
			output(0).push(p);
		}	
	}
	unsigned p = _period * 1000;
	unsigned max_jitter = p / 7;
	long r2 = random();
	unsigned j = (unsigned) ((r2 >> 1) % (max_jitter + 1));
	unsigned int delta_us = 1000 * ((r2 & 1) ? p - j : p + j);
	_timer.schedule_at(Timestamp::now() + Timestamp::make_usec(delta_us));
}


enum {H_DEBUG, H_IP};

static String 
SR1GatewayResponder_read_param(Element *e, void *thunk)
{
  SR1GatewayResponder *td = (SR1GatewayResponder *)e;
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
SR1GatewayResponder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SR1GatewayResponder *f = (SR1GatewayResponder *)e;
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
SR1GatewayResponder::add_handlers()
{
  add_read_handler("debug", SR1GatewayResponder_read_param, (void *) H_DEBUG);
  add_read_handler("ip", SR1GatewayResponder_read_param, (void *) H_IP);
  add_write_handler("debug", SR1GatewayResponder_write_param, (void *) H_DEBUG);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SR1GatewayResponder::IPAddress>;
template class DEQueue<SR1GatewayResponder::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SR1GatewayResponder)

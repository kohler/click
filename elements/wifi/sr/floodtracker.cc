/*
 * FloodTracker.{cc,hh} -- DSR implementation
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
#include "floodtracker.hh"

CLICK_DECLS

FloodTracker::FloodTracker()
{
}

FloodTracker::~FloodTracker()
{
}

int
FloodTracker::configure (Vector<String> &conf, ErrorHandler *errh)
{
	int ret;
	ret = cp_va_parse(conf, this, errh,
			  cpKeywords,
			  cpEnd);
	
	return ret;
}

Packet *
FloodTracker::simple_action(Packet *p_in)
{
	click_ether *eh = (click_ether *) p_in->data();
	struct srpacket *pk = (struct srpacket *) (eh+1);
	
	IPAddress ip = pk->get_link_node(0);
	int si = 0;
	uint32_t seq = pk->seq();
	
	for(si = 0; si < _seen.size(); si++){
		if (ip == _seen[si]._ip && seq == _seen[si]._seq){
			_seen[si]._count++;
			return p_in;
		}
	}
	
	if (si == _seen.size()) {
		if (_seen.size() == 100) {
			_seen.pop_front();
			si--;
		}
		_seen.push_back(Seen(ip, seq));
	}
	_seen[si]._count++;
	
	IPInfo *nfo = _gateways.findp(ip);
	if (!nfo) {
		_gateways.insert(ip, IPInfo());
		nfo = _gateways.findp(ip);
		nfo->_first_update = Timestamp::now();
	}
	
	nfo->_ip = ip;
	nfo->_last_update = Timestamp::now();
	nfo->_seen++;
	
	return p_in;
}


String
FloodTracker::print_gateway_stats()
{
	StringAccum sa;
	Timestamp now = Timestamp::now();
	for(IPIter iter = _gateways.begin(); iter; iter++) {
		IPInfo nfo = iter.value();
		sa << nfo._ip.s() << " ";
		sa << "seen " << nfo._seen << " ";
		sa << "first_update " << now - nfo._first_update << " ";
		sa << "last_update " << now - nfo._last_update << " ";
		sa << "\n";
	}
	
	return sa.take_string();
	
}

enum { H_STATS };
String
FloodTracker::read_param(Element *e, void *vparam)
{
	FloodTracker *f = (FloodTracker *) e;
	switch ((int)vparam) {
	case H_STATS: return f->print_gateway_stats();
	default:
		return "";
	}
	
}

void
FloodTracker::add_handlers()
{
	add_read_handler("stats", read_param, (void *) H_STATS);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<FloodTracker::IPAddress>;
template class DEQueue<FloodTracker::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodTracker)

/*
 * aggregateipflows.{cc,hh} -- set aggregate annotation based on IP addr pair
 * Eddie Kohler
 *
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2005 Regents of the University of California
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
#include "aggregateipaddrpair.hh"
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/handlercall.hh>
CLICK_DECLS

#define SEC_OLDER(s1, s2)	((int)(s1 - s2) < 0)


static inline bool
operator==(const AggregateIPAddrPair::HostPair &a, const AggregateIPAddrPair::HostPair &b)
{
    return a.a == b.a && a.b == b.b;
}

inline hashcode_t
AggregateIPAddrPair::HostPair::hashcode() const
{
    return (a << 12) + b + ((a >> 20) & 0x1F);
}


// actual AggregateIPAddrPair operations

AggregateIPAddrPair::AggregateIPAddrPair()
{
}

AggregateIPAddrPair::~AggregateIPAddrPair()
{
}

void *
AggregateIPAddrPair::cast(const char *n)
{
    if (strcmp(n, "AggregateNotifier") == 0)
        return (AggregateNotifier *)this;
    else if (strcmp(n, "AggregateIPAddrPair") == 0)
	return (Element *)this;
    else
	return Element::cast(n);
}

int
AggregateIPAddrPair::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _timeout = 0;
    _gc_interval = 20 * 60;

    if (Args(conf, this, errh)
	.read("TIMEOUT", SecondsArg(), _timeout)
	.read("REAP", SecondsArg(), _gc_interval)
	.complete() < 0)
	return -1;

    return 0;
}

int
AggregateIPAddrPair::initialize(ErrorHandler *)
{
    _next = 1;
    _active_sec = _gc_sec = 0;
    _timestamp_warning = false;

    return 0;
}

void
AggregateIPAddrPair::reap()
{
    if (_gc_sec) {
	uint32_t timeout = _active_sec - _timeout;

	Vector<uint32_t> to_free;
	for (Map::iterator iter = _map.begin(); iter.live(); iter++) {
	    FlowInfo *finfo = &iter.value();
	    if (SEC_OLDER(finfo->last_timestamp.sec(), timeout)) {
		notify(finfo->aggregate, AggregateListener::DELETE_AGG, 0);
		to_free.push_back(iter.key().a);
		to_free.push_back(iter.key().b);
	    }
	}

	for (uint32_t *u = to_free.begin(); u < to_free.end(); u += 2)
	    _map.erase(*(const HostPair *)u);
    }
    _gc_sec = _active_sec + _gc_interval;
}

// XXX timing when fragments are merged back in?

Packet *
AggregateIPAddrPair::simple_action(Packet *p)
{
    if (p->has_network_header()) {
	const click_ip *iph = p->ip_header();
	HostPair hosts(iph->ip_src.s_addr, iph->ip_dst.s_addr);
	FlowInfo *finfo = &_map[hosts];

	if (_timeout > 0) {
	    // assign timestamp if no timestamp given
	    if (!p->timestamp_anno()) {
		if (!_timestamp_warning) {
		    click_chatter("%p{element}: warning: packet received without timestamp", this);
		    _timestamp_warning = true;
		}
		p->timestamp_anno().assign_now();
	    }

	    if (finfo->aggregate && SEC_OLDER(finfo->last_timestamp.sec(), p->timestamp_anno().sec() - _timeout)) {
		notify(finfo->aggregate, AggregateListener::DELETE_AGG, 0);
		finfo->aggregate = 0;
	    }
	}

	if (!finfo->aggregate) {
	    finfo->aggregate = _next;
	    finfo->reverse = (hosts.a != iph->ip_src.s_addr);
	    if (++_next == 0)
		++_next;
	    notify(finfo->aggregate, AggregateListener::NEW_AGG, p);
	}

	if (_timeout > 0) {
	    finfo->last_timestamp = p->timestamp_anno();
	    _active_sec = p->timestamp_anno().sec();
	    if (_active_sec > _gc_sec)
		reap();
	}

	SET_AGGREGATE_ANNO(p, finfo->aggregate);
	int paint = finfo->reverse ^ (hosts.a != iph->ip_src.s_addr);
	SET_PAINT_ANNO(p, paint);
	return p;

    } else {
	checked_output_push(1, p);
	return 0;
    }
}

enum { H_CLEAR };

int
AggregateIPAddrPair::write_handler(const String &, Element *e, void *thunk, ErrorHandler *)
{
    AggregateIPAddrPair *af = static_cast<AggregateIPAddrPair *>(e);
    switch ((intptr_t)thunk) {
      case H_CLEAR: {
	  int active_sec = af->_active_sec, gc_sec = af->_gc_sec;
	  af->_active_sec = af->_gc_sec = 0x7FFFFFFF;
	  af->reap();
	  af->_active_sec = active_sec, af->_gc_sec = gc_sec;
	  return 0;
      }
      default:
	return -1;
    }
}

void
AggregateIPAddrPair::add_handlers()
{
    add_write_handler("clear", write_handler, H_CLEAR);
}

ELEMENT_REQUIRES(userlevel AggregateNotifier)
EXPORT_ELEMENT(AggregateIPAddrPair)
CLICK_ENDDECLS

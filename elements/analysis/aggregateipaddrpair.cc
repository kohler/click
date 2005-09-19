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
#include <click/hashmap.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
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

static inline uint32_t
hashcode(const AggregateIPAddrPair::HostPair &a)
{
    return (a.a << 12) + a.b + ((a.a >> 20) & 0x1F);
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
    
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "TIMEOUT", cpSeconds, "timeout for address pairs", &_timeout,
		    "REAP", cpSeconds, "garbage collection interval", &_gc_interval,
		    cpEnd) < 0)
	return -1;
    
    return 0;
}

int
AggregateIPAddrPair::initialize(ErrorHandler *errh)
{
    _next = 1;
    _active_sec = _gc_sec = 0;
    _timestamp_warning = false;
    
    return 0;
}

inline void
AggregateIPAddrPair::packet_emit_hook(const Packet *p, const click_ip *iph, FlowInfo *finfo)
{
    // check whether this indicates the flow is over
    if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)
	/* 3.Feb.2004 - NLANR dumps do not contain full TCP headers! So relax
	   the following length check to just make sure the flags are
	   there. */
	&& p->transport_length() >= 14
	&& PAINT_ANNO(p) < 2) {	// ignore ICMP errors
	if (p->tcp_header()->th_flags & TH_RST)
	    finfo->_flow_over = 3;
	else if (p->tcp_header()->th_flags & TH_FIN)
	    finfo->_flow_over |= (1 << PAINT_ANNO(p));
	else if (p->tcp_header()->th_flags & TH_SYN)
	    finfo->_flow_over = 0;
    }

    // count packets
    if (stats() && PAINT_ANNO(p) < 2) {
	StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
	sinfo->_packets[PAINT_ANNO(p)]++;
    }
}

void
AggregateIPAddrPair::assign_aggregate(Map &table, HostPairInfo *hpinfo, int emit_before_sec)
{
    Packet *first = hpinfo->_fragment_head;
    uint16_t want_ip_id = good_ip_header(first)->ip_id;

    // find FlowInfo
    FlowInfo *finfo = 0;
    if (AGGREGATE_ANNO(first)) {
	for (finfo = hpinfo->_flows; finfo && finfo->_aggregate != AGGREGATE_ANNO(first); finfo = finfo->_next)
	    /* nada */;
    } else {
	for (Packet *p = first; !finfo && p; p = p->next()) {
	    const click_ip *iph = good_ip_header(p);
	    if (iph->ip_id == want_ip_id) {
		if (IP_FIRSTFRAG(iph)) {
		    uint32_t ports = *(reinterpret_cast<const uint32_t *>(reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2)));
		    if (PAINT_ANNO(p) & 1)
			ports = flip_ports(ports);
		    finfo = find_flow_info(table, hpinfo, ports, PAINT_ANNO(p) & 1, p);
		}
	    }
	}
    }

    // if no FlowInfo found, delete packet
    if (!finfo) {
	hpinfo->_fragment_head = first->next();
	first->kill();
	return;
    }

    // emit packets at the beginning of the list that have the same IP ID
    const click_ip *iph;
    while (first && (iph = good_ip_header(first)) && iph->ip_id == want_ip_id
	   && (SEC_OLDER(first->timestamp_anno().sec(), emit_before_sec)
	       || !IP_ISFRAG(iph))) {
	Packet *p = first;
	hpinfo->_fragment_head = first = p->next();
	p->set_next(0);
	bool was_fragment = IP_ISFRAG(iph);

	SET_AGGREGATE_ANNO(p, finfo->aggregate());

	// actually emit packet
	if (finfo->reverse())
	    SET_PAINT_ANNO(p, PAINT_ANNO(p) ^ 1);
	finfo->_last_timestamp = p->timestamp_anno();

	packet_emit_hook(p, iph, finfo);
	
	output(0).push(p);

	// if not a fragment, know we don't have more fragments
	if (!was_fragment)
	    return;
    }
    
    // assign aggregate annotation to other packets with the same IP ID
    for (Packet *p = first; p; p = p->next())
	if (good_ip_header(p)->ip_id == want_ip_id)
	    SET_AGGREGATE_ANNO(p, finfo->aggregate());
}

void
AggregateIPAddrPair::reap_map(Map &table, uint32_t timeout, uint32_t done_timeout)
{
    timeout = _active_sec - timeout;
    done_timeout = _active_sec - done_timeout;
    int frag_timeout = _active_sec - _fragment_timeout;

    // free completed flows and emit fragments
    for (Map::iterator iter = table.begin(); iter; iter++) {
	HostPairInfo *hpinfo = &iter.value();
	// fragments
	while (hpinfo->_fragment_head && hpinfo->_fragment_head->timestamp_anno().sec() < frag_timeout)
	    assign_aggregate(table, hpinfo, frag_timeout);

	// can't delete any flows if there are fragments
	if (hpinfo->_fragment_head)
	    continue;

	// completed flows
	FlowInfo **pprev = &hpinfo->_flows;
	FlowInfo *f = *pprev;
	while (f) {
	    // circular comparison
	    if (SEC_OLDER(f->_last_timestamp.sec(), (f->_flow_over == 3 ? done_timeout : timeout))) {
		notify(f->_aggregate, AggregateListener::DELETE_AGG, 0);
		*pprev = f->_next;
		delete_flowinfo(iter.key(), f);
	    } else
		pprev = &f->_next;
	    f = *pprev;
	}
	// XXX never free host pairs
    }
}

void
AggregateIPAddrPair::reap()
{
    if (_gc_sec) {
	reap_map(_tcp_map, _tcp_timeout, _tcp_done_timeout);
	reap_map(_udp_map, _udp_timeout, _udp_timeout);
    }
    _gc_sec = _active_sec + _gc_interval;
}

// XXX timing when fragments are merged back in?

Packet *
AggregateIPAddrPair::simple_action(Packet *p)
{
    const click_ip *iph = p->ip_header();

    if (iph) {

	HostPair hosts(iph->ip_src.s_addr, iph->ip_dst.s_addr);
	int paint = 0;
	if (hosts.a != iph->ip_src.s_addr)
	    paint ^= 1;
	HostPairInfo *hpinfo = m.findp_force(hosts);

	if (!hpinfo->aggregate) {
	    hpinfo->aggregate = next | paint;
	    if (++next == 0)
		++next;
	}

	if (_timeout > 0) {
	    // assign timestamp if no timestamp given
	    if (!p->timestamp_anno()) {
		if (!_timestamp_warning) {
		    click_chatter("%{element}: warning: packet received without timestamp", this);
		    _timestamp_warning = true;
		}
		p->timestamp_anno().set_now();
	    }

	    hpinfo->timestamp = p->timestamp_anno();
	    _active_sec = p->timestamp_anno().sec();
	    if (_active_sec > _gc_sec)
		reap();
	}

	SET_AGGREGATE_ANNO(p, hpinfo->aggregate ^ paint);
	return p;
	
    } else
	checked_output_push(1, p);
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
    add_write_handler("clear", write_handler, (void *)H_CLEAR);
}

ELEMENT_REQUIRES(userlevel AggregateNotifier false)
EXPORT_ELEMENT(AggregateIPAddrPair)
#include <click/bighashmap.cc>
CLICK_ENDDECLS

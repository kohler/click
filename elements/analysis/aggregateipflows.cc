// -*- mode: c++; c-basic-offset: 4 -*-
#include <config.h>
#include <click/config.h>

#include "aggregateipflows.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateIPFlows::AggregateIPFlows()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

AggregateIPFlows::~AggregateIPFlows()
{
    MOD_DEC_USE_COUNT;
}

void *
AggregateIPFlows::cast(const char *n)
{
    if (strcmp(n, "AggregateNotifier") == 0)
	return (AggregateNotifier *)this;
    else if (strcmp(n, "AggregateIPFlows") == 0)
	return (Element *)this;
    else
	return Element::cast(n);
}

void
AggregateIPFlows::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
AggregateIPFlows::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _tcp_timeout = 24 * 60 * 60;
    _tcp_done_timeout = 30;
    _udp_timeout = 60;
    _gc_interval = 20 * 60;
    bool handle_icmp_errors = false;
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "TCP_TIMEOUT", cpSeconds, "timeout for active TCP connections", &_tcp_timeout,
		    "TCP_DONE_TIMEOUT", cpSeconds, "timeout for completed TCP connections", &_tcp_done_timeout,
		    "UDP_TIMEOUT", cpSeconds, "timeout for UDP connections", &_udp_timeout,
		    "REAP", cpSeconds, "garbage collection interval", &_gc_interval,
		    "ICMP", cpBool, "handle ICMP errors?", &handle_icmp_errors,
		    0) < 0)
	return -1;
    _smallest_timeout = (_tcp_timeout < _tcp_done_timeout ? _tcp_timeout : _tcp_done_timeout);
    _smallest_timeout = (_smallest_timeout < _udp_timeout ? _smallest_timeout : _udp_timeout);
    _handle_icmp_errors = handle_icmp_errors;
    return 0;
}

int
AggregateIPFlows::initialize(ErrorHandler *)
{
    _next = 1;
    _active_sec = _gc_sec = 0;
    return 0;
}

void
AggregateIPFlows::clean_map(Map &table, uint32_t timeout, uint32_t done_timeout)
{
    FlowInfo *to_free = 0;
    timeout = _active_sec - timeout;
    done_timeout = _active_sec - done_timeout;

    for (Map::Iterator iter = table.first(); iter; iter++)
	if (!iter.value().reverse()) {
	    FlowInfo *finfo = const_cast<FlowInfo *>(&iter.value());
	    // circular comparison
	    if ((int32_t)(finfo->uu.active_sec - (finfo->flow_over == 3 ? done_timeout : timeout)) < 0) {
		finfo->uu.other = to_free;
		to_free = finfo;
	    }
	}

    while (to_free) {
	FlowInfo *next = to_free->uu.other;
	notify(to_free->_aggregate, AggregateListener::DELETE_AGG, 0);
	IPFlowID flow = table.key_of_value(to_free);
	table.remove(flow);
	table.remove(flow.rev());
	to_free = next;
    }
}

void
AggregateIPFlows::reap()
{
    if (_gc_sec) {
	clean_map(_tcp_map, _tcp_timeout, _tcp_done_timeout);
	clean_map(_udp_map, _udp_timeout, _udp_timeout);
    }
    _gc_sec = _active_sec + _gc_interval;
}

const click_ip *
AggregateIPFlows::icmp_encapsulated_header(const Packet *p) const
{
    const icmp_generic *icmph = reinterpret_cast<const icmp_generic *>(p->transport_header());
    if (icmph
	&& (icmph->icmp_type == ICMP_DST_UNREACHABLE
	    || icmph->icmp_type == ICMP_TYPE_TIME_EXCEEDED
	    || icmph->icmp_type == ICMP_PARAMETER_PROBLEM
	    || icmph->icmp_type == ICMP_SOURCE_QUENCH
	    || icmph->icmp_type == ICMP_REDIRECT)) {
	const click_ip *embedded_iph = reinterpret_cast<const click_ip *>(icmph + 1);
	unsigned embedded_hlen = embedded_iph->ip_hl << 2;
	if ((unsigned)p->transport_length() >= sizeof(icmp_generic) + embedded_hlen
	    && embedded_hlen >= sizeof(click_ip))
	    return embedded_iph;
	else
	    return 0;
    } else
	return 0;
}

Packet *
AggregateIPFlows::simple_action(Packet *p)
{
    const click_ip *iph = p->ip_header();
    int paint = 0;
    
    if (iph && iph->ip_p == IP_PROTO_ICMP && IP_FIRSTFRAG(iph)
	&& _handle_icmp_errors) {
	iph = icmp_encapsulated_header(p);
	paint = 2;
    }
    if (!iph || !IP_FIRSTFRAG(iph)
	|| (iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP)
	|| (reinterpret_cast<const unsigned char *>(iph) - p->data()) + (iph->ip_hl << 2) + sizeof(click_udp) > p->length()) {
	checked_output_push(1, p);
	return 0;
    }

    // find relevant FlowInfo
    IPFlowID flow(iph);
    Map &m = (iph->ip_p == IP_PROTO_TCP ? _tcp_map : _udp_map);
    FlowInfo *finfo = m.findp_force(flow);
    uint32_t old_next = _next;
    unsigned p_sec = p->timestamp_anno().tv_sec;

    if (!finfo) {
	click_chatter("out of memory!");
	checked_output_push(1, p);
	return 0;
    } else if (finfo->fresh()) {
	finfo->_aggregate = _next;
	FlowInfo *rfinfo = m.findp_force(flow.rev());
	rfinfo->uu.other = finfo;
	rfinfo->_reverse = true;
	_next++;		// XXX check for 2^32
	goto flow_is_fresh;
    } else if (finfo->reverse()) {
	finfo = finfo->uu.other;
	paint |= 1;
    }

    // check whether flow is old; if so, we'll use a new number
    if (p_sec && p_sec > finfo->uu.active_sec + _smallest_timeout) {
	unsigned timeout;
	if (iph->ip_p == IP_PROTO_UDP)
	    timeout = _udp_timeout;
	else if (finfo->flow_over == 3)
	    timeout = _tcp_done_timeout;
	else
	    timeout = _tcp_timeout;
	if (p_sec > finfo->uu.active_sec + timeout) {
	    // old flow; kill old aggregate and create new aggregate
	    notify(finfo->_aggregate, AggregateListener::DELETE_AGG, 0);
	    if (paint & 1) {	// switch sides
		FlowInfo *rfinfo = m.findp(flow);
		assert(rfinfo && rfinfo != finfo);
		finfo->_aggregate = 0;
		finfo->uu.other = rfinfo;
		finfo->_reverse = true;
		rfinfo->_reverse = false;
		finfo = rfinfo;
		paint &= ~1;
	    }
	    finfo->_aggregate = _next;
	    finfo->flow_over = 0;
	    _next++;
	}
    }

  flow_is_fresh:
    if (p_sec)
	_active_sec = finfo->uu.active_sec = p_sec;

    // check whether this indicates the flow is over
    if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)
	&& p->transport_length() >= (int)sizeof(click_tcp)
	&& paint < 2) {		// ignore ICMP errors
	if (p->tcp_header()->th_flags & TH_RST)
	    finfo->flow_over = 3;
	else if (p->tcp_header()->th_flags & TH_FIN)
	    finfo->flow_over |= (1 << paint);
	else if (p->tcp_header()->th_flags & TH_SYN)
	    finfo->flow_over = 0;
    }

    // mark packet with aggregate number and paint
    SET_AGGREGATE_ANNO(p, finfo->aggregate());
    SET_PAINT_ANNO(p, paint);

    // notify about the new flow if necessary
    if (_next != old_next)
	notify(_next - 1, AggregateListener::NEW_AGG, p);
    // GC if necessary
    if (_active_sec >= _gc_sec)
	reap();
    
    return p;
}

ELEMENT_REQUIRES(userlevel AggregateNotifier)
EXPORT_ELEMENT(AggregateIPFlows)
#include <click/bighashmap.cc>
CLICK_ENDDECLS

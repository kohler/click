// -*- mode: c++; c-basic-offset: 4 -*-
#include <config.h>
#include <click/config.h>

#include "aggregateflows.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>
#include <click/packet_anno.hh>

AggregateFlows::AggregateFlows()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

AggregateFlows::~AggregateFlows()
{
    MOD_DEC_USE_COUNT;
}

void
AggregateFlows::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
AggregateFlows::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    _bidi = false;
    _ports = true;
    return cp_va_parse(conf, this, errh,
		       cpKeywords,
		       "BIDI", cpBool, "bidirectional?", &_bidi,
		       "PORTS", cpBool, "use ports?", &_ports,
		       0);
}

int
AggregateFlows::initialize(ErrorHandler *)
{
    _next = 1;
    return 0;
}

Packet *
AggregateFlows::simple_action(Packet *p)
{
    const click_ip *iph = p->ip_header();
    if (!iph || (_ports && iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP)) {
	checked_output_push(1, p);
	return 0;
    }

    IPFlowID flow;
    if (_ports)
	flow = IPFlowID(p);
    else
	flow = IPFlowID(iph->ip_src, 0, iph->ip_dst, 0);

    Map &m = (iph->ip_p == IP_PROTO_TCP && _ports ? _tcp_map : _udp_map);
    
    uint32_t agg = m.find(flow);
    if (!agg && _bidi)
	agg = m.find(flow.rev());
    if (!agg) {
	agg = _next;
	m.insert(flow, agg);
	_next++;
    }

    SET_AGGREGATE_ANNO(p, agg);
    return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateFlows)

#include <click/bighashmap.cc>

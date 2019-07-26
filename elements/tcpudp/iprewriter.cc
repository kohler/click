/*
 * iprewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2008-2010 Meraki, Inc.
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
#include "iprewriter.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/router.hh>
CLICK_DECLS

IPRewriter::IPRewriter()
    : _udp_map(0)
{
}

IPRewriter::~IPRewriter()
{
}

void *
IPRewriter::cast(const char *n)
{
    if (strcmp(n, "IPRewriterBase") == 0)
	return (IPRewriterBase *)this;
    else if (strcmp(n, "TCPRewriter") == 0)
	return (TCPRewriter *)this;
    else if (strcmp(n, "IPRewriter") == 0)
	return this;
    else
	return 0;
}

int
IPRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool has_udp_streaming_timeout = false;
    _udp_timeouts[0] = 60 * 5;	// 5 minutes
    _udp_timeouts[1] = 5;	// 5 seconds

    if (Args(this, errh).bind(conf)
	.read("UDP_TIMEOUT", SecondsArg(), _udp_timeouts[0])
	.read("UDP_STREAMING_TIMEOUT", SecondsArg(), _udp_streaming_timeout).read_status(has_udp_streaming_timeout)
	.read("UDP_GUARANTEE", SecondsArg(), _udp_timeouts[1])
	.consume() < 0)
	return -1;

    if (!has_udp_streaming_timeout)
	_udp_streaming_timeout = _udp_timeouts[0];
    _udp_timeouts[0] *= CLICK_HZ; // change timeouts to jiffies
    _udp_timeouts[1] *= CLICK_HZ;
    _udp_streaming_timeout *= CLICK_HZ; // IPRewriterBase handles the others

    return TCPRewriter::configure(conf, errh);
}

IPRewriterEntry *
IPRewriter::get_entry(int ip_p, const IPFlowID &flowid, int input)
{
    if (ip_p == IP_PROTO_TCP)
	return TCPRewriter::get_entry(ip_p, flowid, input);
    if (ip_p != IP_PROTO_UDP)
	return 0;
    IPRewriterEntry *m = _udp_map.get(flowid);
    if (!m && (unsigned) input < (unsigned) _input_specs.size()) {
	IPRewriterInput &is = _input_specs[input];
	IPFlowID rewritten_flowid = IPFlowID::uninitialized_t();
	if (is.rewrite_flowid(flowid, rewritten_flowid, 0, IPRewriterInput::mapid_iprewriter_udp) == rw_addmap)
	    m = IPRewriter::add_flow(0, flowid, rewritten_flowid, input);
    }
    return m;
}

IPRewriterEntry *
IPRewriter::add_flow(int ip_p, const IPFlowID &flowid,
		     const IPFlowID &rewritten_flowid, int input)
{
    if (ip_p == IP_PROTO_TCP)
	return TCPRewriter::add_flow(ip_p, flowid, rewritten_flowid, input);

    void *data;
    if (!(data = _udp_allocator.allocate()))
	return 0;

    IPRewriterInput *rwinput = &_input_specs[input];
    IPRewriterFlow *flow = new(data) IPRewriterFlow
	(rwinput, flowid, rewritten_flowid, ip_p,
	 !!_udp_timeouts[1], click_jiffies() + relevant_timeout(_udp_timeouts));

    return store_flow(flow, input, _udp_map, &reply_udp_map(rwinput));
}

void
IPRewriter::push(int port, Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    click_ip *iph = p->ip_header();

    // handle non-first fragments
    if ((iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP)
	|| !IP_FIRSTFRAG(iph)
	|| p->transport_length() < 8) {
	const IPRewriterInput &is = _input_specs[port];
	if (is.kind == IPRewriterInput::i_nochange)
	    output(is.foutput).push(p);
	else
	    p->kill();
	return;
    }

    IPFlowID flowid(p);
    HashContainer<IPRewriterEntry> *map = (iph->ip_p == IP_PROTO_TCP ? &_map : &_udp_map);
    IPRewriterEntry *m = map->get(flowid);

    if (!m) {			// create new mapping
	IPRewriterInput &is = _input_specs.unchecked_at(port);
	IPFlowID rewritten_flowid = IPFlowID::uninitialized_t();
	int result = is.rewrite_flowid(flowid, rewritten_flowid, p, iph->ip_p == IP_PROTO_TCP ? 0 : IPRewriterInput::mapid_iprewriter_udp);
	if (result == rw_addmap)
	    m = IPRewriter::add_flow(iph->ip_p, flowid, rewritten_flowid, port);
	if (!m) {
	    checked_output_push(result, p);
	    return;
	} else if (_annos & 2)
	    m->flow()->set_reply_anno(p->anno_u8(_annos >> 2));
    }

    click_jiffies_t now_j = click_jiffies();
    IPRewriterFlow *mf = m->flow();
    if (iph->ip_p == IP_PROTO_TCP) {
	TCPFlow *tcpmf = static_cast<TCPFlow *>(mf);
	tcpmf->apply(p, m->direction(), _annos);
	if (_timeouts[1])
	    tcpmf->change_expiry(_heap, true, now_j + _timeouts[1]);
	else
	    tcpmf->change_expiry(_heap, false, now_j + tcp_flow_timeout(tcpmf));
    } else {
	UDPFlow *udpmf = static_cast<UDPFlow *>(mf);
	udpmf->apply(p, m->direction(), _annos);
	if (_udp_timeouts[1])
	    udpmf->change_expiry(_heap, true, now_j + _udp_timeouts[1]);
	else
	    udpmf->change_expiry(_heap, false, now_j + udp_flow_timeout(udpmf));
    }

    output(m->output()).push(p);
}

String
IPRewriter::udp_mappings_handler(Element *e, void *)
{
    IPRewriter *rw = (IPRewriter *)e;
    click_jiffies_t now = click_jiffies();
    StringAccum sa;
    for (Map::iterator iter = rw->_udp_map.begin(); iter.live(); ++iter) {
	iter->flow()->unparse(sa, iter->direction(), now);
	sa << '\n';
    }
    return sa.take_string();
}

void
IPRewriter::add_handlers()
{
    add_read_handler("tcp_table", tcp_mappings_handler);
    add_read_handler("udp_table", udp_mappings_handler);
    add_read_handler("tcp_mappings", tcp_mappings_handler, 0, Handler::h_deprecated);
    add_read_handler("udp_mappings", udp_mappings_handler, 0, Handler::h_deprecated);
    set_handler("tcp_lookup", Handler::OP_READ | Handler::READ_PARAM, tcp_lookup_handler, 0);
    add_rewriter_handlers(true);
}

void
IPRewriter::destroy_flow(IPRewriterFlow *flow)
{
	if (flow->ip_p() == IP_PROTO_TCP)
		TCPRewriter::destroy_flow(flow);
	else {
		unmap_flow(flow, _udp_map, &reply_udp_map(flow->owner()));
		flow->~IPRewriterFlow();
		_udp_allocator.deallocate(flow);
	}
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPRewriter UDPRewriter)
EXPORT_ELEMENT(IPRewriter)

/*
 * ipaddrpairrewriter.{cc,hh} -- rewrites packet source and destination
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
 * Copyright (c) 2009-2010 Meraki, Inc.
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
#include "ipaddrpairrewriter.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

void
IPAddrPairRewriter::IPAddrPairFlow::apply(WritablePacket *p, bool direction,
					  unsigned annos)
{
    assert(p->has_network_header());
    click_ip *iph = p->ip_header();

    // IP header
    const IPFlowID &revflow = _e[!direction].flowid();
    iph->ip_src = revflow.daddr();
    iph->ip_dst = revflow.saddr();
    if (annos & 1)
	p->set_dst_ip_anno(revflow.saddr());
    if (direction && (annos & 2))
	p->set_anno_u8(annos >> 2, _reply_anno);
    update_csum(&iph->ip_sum, direction, _ip_csum_delta);

    // UDP/TCP header
    if (!IP_FIRSTFRAG(iph))
	/* do nothing */;
    else if (iph->ip_p == IP_PROTO_TCP && p->transport_length() >= 18) {
	click_tcp *tcph = p->tcp_header();
	update_csum(&tcph->th_sum, direction, _udp_csum_delta);
    } else if (iph->ip_p == IP_PROTO_UDP && p->transport_length() >= 8) {
	click_udp *udph = p->udp_header();
	if (udph->uh_sum)	// 0 checksum is no checksum
	    update_csum(&udph->uh_sum, direction, _udp_csum_delta);
    }
}

void
IPAddrPairRewriter::IPAddrPairFlow::unparse(StringAccum &sa, bool direction,
					    click_jiffies_t now) const
{
    const IPFlowID &flow = _e[direction].flowid();
    const IPFlowID &reply_flow = _e[!direction].flowid();
    sa << '(' << flow.saddr() << ", " << flow.daddr() << ") => ("
       << reply_flow.daddr() << ", " << reply_flow.saddr() << ')';
    unparse_ports(sa, direction, now);
}

IPAddrPairRewriter::IPAddrPairRewriter()
{
}

IPAddrPairRewriter::~IPAddrPairRewriter()
{
}

void *
IPAddrPairRewriter::cast(const char *n)
{
    if (strcmp(n, "IPRewriterBase") == 0)
	return (IPRewriterBase *)this;
    else if (strcmp(n, "IPAddrPairRewriter") == 0)
	return (IPAddrPairRewriter *)this;
    else
	return 0;
}

int
IPAddrPairRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool has_reply_anno = false;
    int reply_anno;
    _timeouts[0] = 60 * 120;	// 2 hours

    if (Args(this, errh).bind(conf)
	.read("REPLY_ANNO", has_reply_anno, AnnoArg(1), reply_anno)
	.consume() < 0)
	return -1;

    _annos = 1 + (has_reply_anno ? 2 + (reply_anno << 2) : 0);
    return IPRewriterBase::configure(conf, errh);
}

IPRewriterEntry *
IPAddrPairRewriter::get_entry(int, const IPFlowID &xflowid, int input)
{
    IPFlowID flowid(xflowid.saddr(), 0, xflowid.daddr(), 0);
    IPRewriterEntry *m = _map.get(flowid);
    if (!m && (unsigned) input < (unsigned) _input_specs.size()) {
	IPRewriterInput &is = _input_specs[input];
	IPFlowID rewritten_flowid = IPFlowID::uninitialized_t();
	if (is.rewrite_flowid(flowid, rewritten_flowid, 0) == rw_addmap)
	    m = IPAddrPairRewriter::add_flow(0, flowid, rewritten_flowid, input);
    }
    return m;
}

IPRewriterEntry *
IPAddrPairRewriter::add_flow(int, const IPFlowID &flowid,
			     const IPFlowID &rewritten_flowid, int input)
{
    void *data;
    if (rewritten_flowid.sport()
	|| rewritten_flowid.dport()
	|| !(data = _allocator.allocate()))
	return 0;

    IPAddrPairFlow *flow = new(data) IPAddrPairFlow
	(&_input_specs[input], flowid, rewritten_flowid,
	 !!_timeouts[1], click_jiffies() + relevant_timeout(_timeouts));

    return store_flow(flow, input, _map);
}

void
IPAddrPairRewriter::push(int port, Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    click_ip *iph = p->ip_header();

    IPFlowID flowid(iph->ip_src, 0, iph->ip_dst, 0);
    IPRewriterEntry *m = _map.get(flowid);

    if (!m) {			// create new mapping
	IPRewriterInput &is = _input_specs.unchecked_at(port);
	IPFlowID rewritten_flowid = IPFlowID::uninitialized_t();
	int result = is.rewrite_flowid(flowid, rewritten_flowid, p);
	if (result == rw_addmap)
	    m = IPAddrPairRewriter::add_flow(0, flowid, rewritten_flowid, port);
	if (!m) {
	    checked_output_push(result, p);
	    return;
	} else if (_annos & 2)
	    m->flow()->set_reply_anno(p->anno_u8(_annos >> 2));
    }

    IPAddrPairFlow *mf = static_cast<IPAddrPairFlow *>(m->flow());
    mf->apply(p, m->direction(), _annos);
    mf->change_expiry_by_timeout(_heap, click_jiffies(), _timeouts);
    output(m->output()).push(p);
}


String
IPAddrPairRewriter::dump_mappings_handler(Element *e, void *)
{
    IPAddrPairRewriter *rw = (IPAddrPairRewriter *)e;
    click_jiffies_t now = click_jiffies();
    StringAccum sa;
    for (Map::iterator iter = rw->_map.begin(); iter.live(); iter++) {
	IPAddrPairFlow *f = static_cast<IPAddrPairFlow *>(iter->flow());
	f->unparse(sa, iter->direction(), now);
	sa << '\n';
    }
    return sa.take_string();
}

void
IPAddrPairRewriter::add_handlers()
{
    add_read_handler("table", dump_mappings_handler);
    add_read_handler("mappings", dump_mappings_handler, 0, Handler::h_deprecated);
    add_rewriter_handlers(true);
}

ELEMENT_REQUIRES(IPRewriterBase)
EXPORT_ELEMENT(IPAddrPairRewriter)
CLICK_ENDDECLS
